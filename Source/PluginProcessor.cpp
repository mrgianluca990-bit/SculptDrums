#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <utility>

//==============================================================================
class SculptDrumsProcessor::KitLoader : public juce::Thread
{
public:
    explicit KitLoader (SculptDrumsProcessor& o) : juce::Thread ("Sculpt kit loader"), owner (o) {}
    void run() override { owner.loadKit (sampleRate, *this); }
    double sampleRate = 44100.0;

private:
    SculptDrumsProcessor& owner;
};

// Se due stadi adiacenti ricadono sullo stesso campione (fallback di un
// elemento mancante), evita il +3 dB che darebbe la somma di due copie identiche.
static std::array<float, sculpt::stageCount> collapseDuplicateSources (
    const std::array<const juce::AudioBuffer<float>*, sculpt::stageCount>& sources,
    std::array<float, sculpt::stageCount> weights)
{
    for (int i = 0; i < sculpt::stageCount; ++i)
    {
        if (sources[(size_t) i] == nullptr) { weights[(size_t) i] = 0.0f; continue; }
        for (int earlier = 0; earlier < i; ++earlier)
            if (sources[(size_t) earlier] == sources[(size_t) i])
            {
                weights[(size_t) earlier] = std::hypot (weights[(size_t) earlier], weights[(size_t) i]);
                weights[(size_t) i] = 0.0f;
                break;
            }
    }
    return weights;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SculptDrumsProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "morph", 1 }, "Morph",
           NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 }, "Volume",
           NormalisableRange<float> (-60.0f, 6.0f, 0.1f), 0.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "rrRange", 1 }, "RR Range",
           NormalisableRange<float> (0.0f, 10.0f, 0.1f), 3.0f));
    return l;
}

SculptDrumsProcessor::SculptDrumsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout()),
      loader (std::make_unique<KitLoader> (*this))
{
    pMorph  = apvts.getRawParameterValue ("morph");
    pVolume = apvts.getRawParameterValue ("volume");
    pRange  = apvts.getRawParameterValue ("rrRange");
    for (auto& w : layerWeight) w = 0.0f;
}

SculptDrumsProcessor::~SculptDrumsProcessor() { loader->stopThread (10000); }

bool SculptDrumsProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

juce::String SculptDrumsProcessor::getKitMessage() const
{
    const juce::ScopedLock sl (msgLock);
    return kitMessage;
}

void SculptDrumsProcessor::setKitMessage (const juce::String& m)
{
    const juce::ScopedLock sl (msgLock);
    kitMessage = m;
}

//==============================================================================
// Kit di prova (usato finché kit.json non viene trovato/caricato)
std::vector<SculptDrumsProcessor::Piece> SculptDrumsProcessor::makePlaceholderKit (double sr)
{
    const int notes[3] = { 36, 38, 42 };
    const float gammas[3] = { 1.0f, 0.85f, 1.2f };
    const float lens[3][3] = { { 0.5f, 0.5f, 0.7f }, { 0.35f, 0.35f, 0.4f }, { 0.12f, 0.12f, 0.2f } };
    const float twoPi = juce::MathConstants<float>::twoPi;
    std::vector<Piece> out (3);

    for (int p = 0; p < 3; ++p)
    {
        out[(size_t) p].id = "test" + juce::String (p);
        out[(size_t) p].note = notes[p];
        out[(size_t) p].gamma = gammas[p];
        out[(size_t) p].rr.setStart (p * 3);
        for (auto& layer : out[(size_t) p].layers) layer.resize (1);

        for (int k = 0; k < sculpt::stageCount; ++k)
        {
            auto& b = out[(size_t) p].layers[(size_t) k][0].buf;
            const int family = k <= 1 ? 0 : (k == sculpt::stageCount - 1 ? 2 : 1);
            const int n = (int) (lens[p][family] * (float) sr);
            b.setSize (2, n);
            auto* d = b.getWritePointer (0);
            juce::Random rnd (1234 + p * 10 + k);
            float ph = 0.0f, prev = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                const float t = (float) i / (float) sr;
                const float noise = rnd.nextFloat() * 2.0f - 1.0f;
                float s = 0.0f;
                if (p == 0)
                {
                    ph += twoPi * (45.0f + 90.0f * std::exp (-t * 25.0f)) / (float) sr;
                    s = std::sin (ph) * std::exp (-t * 8.0f);
                    if (family == 1) s = std::tanh (2.5f * s);
                    if (family == 2) s = std::sin (twoPi * 52.0f * t) * std::exp (-t * 5.0f) + (i < 40 ? 0.5f : 0.0f);
                }
                else if (p == 1)
                {
                    s = noise * std::exp (-t * (family == 2 ? 12.0f : 18.0f)) * 0.6f
                      + std::sin (twoPi * (family == 2 ? 220.0f : 190.0f) * t) * std::exp (-t * 30.0f) * 0.5f;
                    if (family == 1) s = std::tanh (2.0f * s);
                }
                else
                {
                    const float hp = noise - prev; prev = noise;
                    s = hp * std::exp (-t * (family == 0 ? 60.0f : family == 1 ? 45.0f : 35.0f)) * 0.5f;
                }
                d[i] = s;
            }
            const float mag = b.getMagnitude (0, 0, n);
            if (mag > 0.0f) b.applyGain (0, 0, n, 0.8f / mag);
            b.copyFrom (1, 0, b, 0, 0, n);
        }
    }
    return out;
}

//==============================================================================
// Caricamento campioni
bool SculptDrumsProcessor::loadSample (juce::AudioFormatManager& fm, const juce::File& f, double sr,
                                       juce::AudioBuffer<float>& out)
{
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 0 || r->lengthInSamples > 20000000) return false;

    const int n = (int) r->lengthInSamples;
    juce::AudioBuffer<float> raw (2, n + 16); // margine a zero per l'interpolatore
    raw.clear();
    r->read (&raw, 0, n, 0, true, true);
    if (r->numChannels == 1) raw.copyFrom (1, 0, raw, 0, 0, n);

    if (std::abs (r->sampleRate - sr) > 0.5)
    {
        const double ratio = r->sampleRate / sr;
        const int outN = (int) std::ceil ((double) n / ratio);
        out.setSize (2, outN);
        for (int ch = 0; ch < 2; ++ch)
        {
            juce::LagrangeInterpolator li;
            li.process (ratio, raw.getReadPointer (ch), out.getWritePointer (ch), outN);
        }
    }
    else
    {
        out.setSize (2, n);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, 0, raw, ch, 0, n);
    }

    const int len = out.getNumSamples();
    out.applyGainRamp (0, 0, juce::jmin (32, len), 0.0f, 1.0f);                       // anti-click in ingresso
    const int fo = juce::jmin (len, (int) (0.01 * sr));
    out.applyGainRamp (0, len - fo, fo, 1.0f, 0.0f);                                  // coda a zero
    return true;
}

// OH e room dello stesso evento vengono sommati insieme al close, mantenendo
// il loro offset temporale originale: non sono varianti round robin indipendenti.
bool SculptDrumsProcessor::loadMicGroup (juce::AudioFormatManager& fm, const juce::File& base,
                                         const juce::var& entry, double sr, juce::AudioBuffer<float>& out)
{
    if (! loadSample (fm, base.getChildFile (entry["file"].toString()), sr, out)) return false;

    for (const auto& mic : { std::pair<const char*, float> { "oh", 0.25f },
                             std::pair<const char*, float> { "room", 0.15f } })
    {
        const auto path = entry.getProperty (juce::Identifier (mic.first), {}).toString();
        if (path.isEmpty()) continue;
        juce::AudioBuffer<float> additional;
        if (! loadSample (fm, base.getChildFile (path), sr, additional)) continue;

        const int samples = juce::jmax (out.getNumSamples(), additional.getNumSamples());
        juce::AudioBuffer<float> mixed (2, samples);
        mixed.clear();
        for (int ch = 0; ch < 2; ++ch)
        {
            mixed.copyFrom (ch, 0, out, ch, 0, out.getNumSamples());
            mixed.addFrom (ch, 0, additional, ch, 0, additional.getNumSamples(), mic.second);
        }
        out = std::move (mixed);
    }
    return true;
}

static float earlyRms (const juce::AudioBuffer<float>& b, int win)
{
    const int n = juce::jmin (win, b.getNumSamples());
    return n > 0 ? 0.5f * (b.getRMSLevel (0, 0, n) + b.getRMSLevel (1, 0, n)) : 0.0f;
}

// Porta i campioni di un livello a un livello percepito simile (RMS dei primi 300 ms),
// ma i campioni con una gamma di velocity propria (pp/p/mp) condividono lo stesso guadagno
// per non perdere la dinamica registrata.
void SculptDrumsProcessor::levelMatchGroup (std::vector<Sample>& g, double sr)
{
    if (g.empty()) return;
    const int win = (int) (0.3 * sr);
    const float target = 0.1f;
    std::vector<float> gains (g.size(), 1.0f), base;

    for (size_t i = 0; i < g.size(); ++i)
    {
        const float r = earlyRms (g[i].buf, win);
        gains[i] = r > 1.0e-5f ? target / r : 1.0f;
        if (g[i].velLo == 0 && g[i].velHi == 127) base.push_back (gains[i]);
    }
    if (base.empty()) base = gains;
    std::sort (base.begin(), base.end());
    const float ref = base[base.size() / 2];

    for (size_t i = 0; i < g.size(); ++i)
    {
        const bool ranged = ! (g[i].velLo == 0 && g[i].velHi == 127);
        float gn = ranged ? ref : gains[i];
        const float pk = g[i].buf.getMagnitude (0, g[i].buf.getNumSamples());
        if (pk * gn > 0.98f && pk > 0.0f) gn = 0.98f / pk;
        g[i].buf.applyGain (gn);
    }
}

void SculptDrumsProcessor::loadKit (double sr, juce::Thread& thread)
{
    const auto root = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Sculpt Drums");
    const auto kitFile = root.getChildFile ("kit.json");

    if (! kitFile.existsAsFile())
    {
        kitStatus = Error;
        setKitMessage ("kit.json non trovato in " + root.getFullPathName() + " - suoni di prova");
        return;
    }

    kitStatus = Loading;
    setKitMessage ("Caricamento campioni...");
    const auto json = juce::JSON::parse (kitFile);
    const juce::var piecesArray = json["pieces"];
    auto* arr = piecesArray.getArray();
    if (arr == nullptr)
    {
        kitStatus = Error;
        setKitMessage ("kit.json non valido - suoni di prova");
        return;
    }

    if ((int) json.getProperty ("version", 0) != 2)
    {
        kitStatus = Error;
        setKitMessage ("kit.json vecchio: serve il manifest a 6 stadi - suoni di prova");
        return;
    }

    const auto folder = root.getChildFile (json.getProperty ("sampleFolder", ".").toString());
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    std::vector<Piece> np;
    int loaded = 0, missing = 0;

    for (auto& pv : *arr)
    {
        if (thread.threadShouldExit()) return;

        Piece p;
        p.id = pv["id"].toString();
        p.note = (int) pv["note"];
        p.gamma = (float) (double) pv.getProperty ("gamma", 1.0);
        p.choke = (int) pv.getProperty ("choke", 0);
        p.rr.setStart ((int) np.size() * 3); // ogni pezzo parte da un punto diverso della sequenza

        const juce::var stagesArray = pv["stages"];
        auto* stages = stagesArray.getArray();
        if (stages == nullptr || stages->size() != sculpt::stageCount) continue;
        for (int layer = 0; layer < sculpt::stageCount; ++layer)
        {
            const juce::var stageArray = (*stages)[layer];
            auto* list = stageArray.getArray();
            if (list == nullptr) continue;
            for (auto& e : *list)
            {
                if (thread.threadShouldExit()) return;
                Sample s;
                if (! loadMicGroup (fm, folder, e, sr, s.buf)) { ++missing; continue; }
                const juce::var velocityRange = e["vel"];
                if (auto* v = velocityRange.getArray(); v != nullptr && v->size() == 2)
                {
                    s.velLo = (int) (*v)[0];
                    s.velHi = (int) (*v)[1];
                }
                ++loaded;
                p.layers[(size_t) layer].push_back (std::move (s));
            }
        }

        bool hasSample = false;
        for (auto& layer : p.layers)
        {
            hasSample |= ! layer.empty();
            levelMatchGroup (layer, sr);
        }
        if (! hasSample) continue;

        np.push_back (std::move (p));
    }

    if (thread.threadShouldExit()) return;
    if (np.empty())
    {
        kitStatus = Error;
        setKitMessage ("Nessun campione trovato in " + folder.getFullPathName() + " - suoni di prova");
        return;
    }

    {
        const juce::SpinLock::ScopedLockType sl (kitLock);
        for (auto& v : voices) v.active = false;
        pieces.swap (np);
    }
    loadedSr = sr;
    kitStatus = Loaded;
    const auto expectedWavs = (int) json.getProperty ("sampleFiles", 0);
    setKitMessage ("Kit: " + juce::String (loaded) + " eventi / "
                   + juce::String (expectedWavs) + " WAV"
                   + (missing > 0 ? ", " + juce::String (missing) + " mancanti" : juce::String()));
}

//==============================================================================
void SculptDrumsProcessor::prepareToPlay (double sr, int)
{
    morphSm.reset (sr, 0.05);
    morphSm.setCurrentAndTargetValue (pMorph->load());
    volSm.reset (sr, 0.02);
    volSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pVolume->load(), -60.0f));

    const bool alreadyLoaded = kitStatus.load() == Loaded && std::abs (loadedSr.load() - sr) < 0.5;
    if (alreadyLoaded) return;

    loader->stopThread (10000);
    {
        const juce::SpinLock::ScopedLockType sl (kitLock);
        for (auto& v : voices) v.active = false;
        pieces = makePlaceholderKit (sr);
    }
    kitStatus = Placeholder;
    setKitMessage ("Suoni di prova");
    loader->sampleRate = sr;
    loader->startThread();
}

int SculptDrumsProcessor::pickVariant (Piece& p, int layer, int vel127)
{
    auto& v = p.layers[(size_t) layer];
    const int n = (int) v.size();
    if (n == 0) return -1;

    for (int k = 0; k < n; ++k) // prossima variante la cui gamma di velocity contiene il colpo
    {
        const int idx = (p.rrCounter[(size_t) layer] + k) % n;
        if (vel127 >= v[(size_t) idx].velLo && vel127 <= v[(size_t) idx].velHi)
        {
            p.rrCounter[(size_t) layer] = (idx + 1) % n;
            return idx;
        }
    }
    int best = 0, bestD = 1000; // nessuna nel range: la più vicina
    for (int i = 0; i < n; ++i)
    {
        const int d = juce::jmin (std::abs (vel127 - v[(size_t) i].velLo), std::abs (vel127 - v[(size_t) i].velHi));
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void SculptDrumsProcessor::trigger (int note, float velocity)
{
    for (size_t pi = 0; pi < pieces.size(); ++pi)
    {
        auto& p = pieces[pi];
        if (p.note != note) continue;

        const int v127 = juce::jlimit (1, 127, (int) std::round (velocity * 127.0f));

        if (p.choke > 0) // charleston: il pedale/chiuso spegne l'aperto e viceversa
            for (auto& v : voices)
                if (v.active && v.piece != (int) pi && pieces[(size_t) v.piece].choke == p.choke) v.choking = true;

        Voice* slot = &voices[0];
        for (auto& v : voices)
        {
            if (! v.active) { slot = &v; break; }
            if (v.pos > slot->pos) slot = &v; // altrimenti ruba la più vecchia
        }

        slot->active = true;
        slot->piece = (int) pi;
        slot->pos = 0;
        slot->choking = false;
        slot->chokeGain = 1.0f;
        slot->vel = std::pow (velocity, 1.4f);
        slot->rrOffset = p.rr.next (pRange->load());
        const auto w = sculpt::morphWeights (sculpt::applyOffset (morphSm.getCurrentValue(), slot->rrOffset), p.gamma);
        slot->src.fill (nullptr);
        std::array<int, sculpt::stageCount> pickedVariants;
        pickedVariants.fill (-1);
        for (int stage = 0; stage < sculpt::stageCount; ++stage)
        {
            int nearest = -1, distance = sculpt::stageCount;
            // Le famiglie non disponibili per un pezzo usano lo stadio più vicino.
            // Non si creano duplicati in RAM e i sei punti MORPH restano validi.
            for (int s = 0; s < sculpt::stageCount; ++s)
                if (! p.layers[(size_t) s].empty() && std::abs (s - stage) < distance)
                {
                    nearest = s;
                    distance = std::abs (s - stage);
                }
            if (nearest >= 0)
            {
                auto& selected = pickedVariants[(size_t) nearest];
                if (selected < 0) selected = pickVariant (p, nearest, v127);
                const int v = selected;
                if (v >= 0)
                    slot->src[(size_t) stage] = &p.layers[(size_t) nearest][(size_t) v].buf;
            }
        }
        slot->w = collapseDuplicateSources (slot->src, w.stage);
        rrStep = p.rr.step();
        return;
    }
}

void SculptDrumsProcessor::renderSegment (juce::AudioBuffer<float>& buf, int start, int n)
{
    if (n <= 0) return;
    const float m1 = morphSm.skip (n);
    const float v0 = volSm.getCurrentValue();
    const float v1 = volSm.skip (n);
    float* L = buf.getWritePointer (0) + start;
    float* R = buf.getWritePointer (1) + start;

    for (auto& v : voices)
    {
        if (! v.active) continue;
        const auto& p = pieces[(size_t) v.piece];
        // L'offset scelto dal round robin rimane fisso per il colpo,
        // mentre il target MORPH può seguire l'automazione della DAW.
        const auto targetWeights = collapseDuplicateSources (
            v.src, sculpt::morphWeights (sculpt::applyOffset (m1, v.rrOffset), p.gamma).stage);
        std::array<const float*, sculpt::stageCount> dl {}, dr {};
        std::array<int, sculpt::stageCount> len {};
        int longest = 0;
        for (int k = 0; k < sculpt::stageCount; ++k)
            if (v.src[(size_t) k] != nullptr)
            {
                dl[(size_t) k] = v.src[(size_t) k]->getReadPointer (0);
                dr[(size_t) k] = v.src[(size_t) k]->getReadPointer (1);
                len[(size_t) k] = v.src[(size_t) k]->getNumSamples();
                longest = juce::jmax (longest, len[(size_t) k]);
            }

        bool dead = false;
        // Crossfade regolare anche durante il decadimento se MORPH cambia.
        for (int i = 0; i < n && ! dead; ++i)
        {
            const float t = (float) i / (float) n;
            const int idx = v.pos + i;
            float sl = 0.0f, sr = 0.0f;
            for (int k = 0; k < sculpt::stageCount; ++k)
                if (idx < len[(size_t) k])
                {
                    const float g = v.w[(size_t) k]
                                  + (targetWeights[(size_t) k] - v.w[(size_t) k]) * t;
                    sl += g * dl[(size_t) k][idx];
                    sr += g * dr[(size_t) k][idx];
                }
            float g = v.vel * (v0 + (v1 - v0) * t) * v.chokeGain;
            if (v.choking)
            {
                v.chokeGain -= 1.0f / 512.0f;
                if (v.chokeGain <= 0.0f) { dead = true; g = 0.0f; }
            }
            L[i] += sl * g;
            R[i] += sr * g;
        }

        v.pos += n;
        v.w = targetWeights;
        if (dead || v.pos >= longest) v.active = false;
    }

    const auto cw = sculpt::morphWeights (m1, 1.0f);
    layerWeight[0] = juce::jmin (1.0f, cw.stage[0] + cw.stage[1]);
    layerWeight[1] = juce::jmin (1.0f, cw.stage[2] + cw.stage[3] + cw.stage[4]);
    layerWeight[2] = cw.stage[5];
}

void SculptDrumsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const juce::SpinLock::ScopedTryLockType lock (kitLock); // il kit può essere sostituito dal caricamento
    if (! lock.isLocked()) return;

    const int N = buffer.getNumSamples();
    morphSm.setTargetValue (pMorph->load());
    volSm.setTargetValue (juce::Decibels::decibelsToGain (pVolume->load(), -60.0f));

    int pos = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int t = juce::jlimit (pos, N, meta.samplePosition);
        renderSegment (buffer, pos, t - pos);
        pos = t;

        if (msg.isNoteOn()) trigger (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) for (auto& v : voices) v.active = false;
    }
    renderSegment (buffer, pos, N - pos);

    const float peak = buffer.getMagnitude (0, N);
    meterLevel = juce::jmax (peak, meterLevel.load() * 0.92f);
}

juce::AudioProcessorEditor* SculptDrumsProcessor::createEditor() { return new SculptDrumsEditor (*this); }

void SculptDrumsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void SculptDrumsProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SculptDrumsProcessor(); }
