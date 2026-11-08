#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

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
        out[(size_t) p].layers[0].resize (1);
        out[(size_t) p].layers[1].resize (1);
        out[(size_t) p].layers[2].resize (1);

        for (int k = 0; k < 3; ++k)
        {
            auto& b = out[(size_t) p].layers[(size_t) k][0].buf;
            const int n = (int) (lens[p][k] * (float) sr);
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
                    if (k == 1) s = std::tanh (2.5f * s);
                    if (k == 2) s = std::sin (twoPi * 52.0f * t) * std::exp (-t * 5.0f) + (i < 40 ? 0.5f : 0.0f);
                }
                else if (p == 1)
                {
                    s = noise * std::exp (-t * (k == 2 ? 12.0f : 18.0f)) * 0.6f
                      + std::sin (twoPi * (k == 2 ? 220.0f : 190.0f) * t) * std::exp (-t * 30.0f) * 0.5f;
                    if (k == 1) s = std::tanh (2.0f * s);
                }
                else
                {
                    const float hp = noise - prev; prev = noise;
                    s = hp * std::exp (-t * (k == 0 ? 60.0f : k == 1 ? 45.0f : 35.0f)) * 0.5f;
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

// Versione "trattata" derivata dal naturale: saturazione in parallelo (compressione/colore),
// a pari livello. Da sostituire con campioni trattati veri quando disponibili.
juce::AudioBuffer<float> SculptDrumsProcessor::makeTreated (const juce::AudioBuffer<float>& in, double sr)
{
    juce::AudioBuffer<float> out (in);
    const float drive = 3.0f, norm = 1.0f / std::tanh (drive);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = out.getWritePointer (ch);
        for (int i = 0; i < out.getNumSamples(); ++i)
            d[i] = 0.75f * std::tanh (drive * d[i]) * norm + 0.25f * d[i];
    }
    const float ro = earlyRms (out, (int) (0.3 * sr));
    if (ro > 1.0e-6f) out.applyGain (earlyRms (in, (int) (0.3 * sr)) / ro);
    const float pk = out.getMagnitude (0, out.getNumSamples());
    if (pk > 0.98f) out.applyGain (0.98f / pk);
    return out;
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
    auto* arr = json["pieces"].getArray();
    if (arr == nullptr)
    {
        kitStatus = Error;
        setKitMessage ("kit.json non valido - suoni di prova");
        return;
    }

    const auto folder = root.getChildFile (json.getProperty ("sampleFolder", "Campioni").toString());
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

        const char* names[3] = { "natural", "treated", "electronic" };
        for (int layer = 0; layer < 3; ++layer)
        {
            auto* list = pv[names[layer]].getArray();
            if (list == nullptr) continue;
            for (auto& e : *list)
            {
                if (thread.threadShouldExit()) return;
                Sample s;
                if (! loadSample (fm, folder.getChildFile (e["file"].toString()), sr, s.buf)) { ++missing; continue; }
                if (auto* v = e["vel"].getArray(); v != nullptr && v->size() == 2)
                {
                    s.velLo = (int) (*v)[0];
                    s.velHi = (int) (*v)[1];
                }
                ++loaded;
                p.layers[(size_t) layer].push_back (std::move (s));
            }
        }

        if (p.layers[0].empty() && ! p.layers[2].empty()) p.layers[0] = p.layers[2]; // solo elettronico (es. clap)
        if (p.layers[2].empty() && ! p.layers[0].empty()) p.layers[2] = p.layers[0]; // solo acustico
        if (p.layers[0].empty()) continue;

        levelMatchGroup (p.layers[0], sr);
        levelMatchGroup (p.layers[2], sr);

        if (p.layers[1].empty()) // trattato derivato dal naturale, stesse varianti e stesse gamme di velocity
            for (auto& s : p.layers[0])
            {
                Sample t;
                t.buf = makeTreated (s.buf, sr);
                t.velLo = s.velLo; t.velHi = s.velHi;
                p.layers[1].push_back (std::move (t));
            }
        else levelMatchGroup (p.layers[1], sr);

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
    setKitMessage ("Kit: " + juce::String (loaded) + " campioni" + (missing > 0 ? ", " + juce::String (missing) + " mancanti" : juce::String()));
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

        const int iN = pickVariant (p, 0, v127);
        const int iT = (p.layers[1].size() == p.layers[0].size() && iN >= 0) ? iN : pickVariant (p, 1, v127);
        const int iE = pickVariant (p, 2, v127);

        slot->active = true;
        slot->piece = (int) pi;
        slot->pos = 0;
        slot->choking = false;
        slot->chokeGain = 1.0f;
        slot->vel = std::pow (velocity, 1.4f);
        slot->rrOffset = p.rr.next (pRange->load());
        slot->src[0] = iN >= 0 ? &p.layers[0][(size_t) iN].buf : nullptr;
        slot->src[1] = iT >= 0 ? &p.layers[1][(size_t) iT].buf : nullptr;
        slot->src[2] = iE >= 0 ? &p.layers[2][(size_t) iE].buf : nullptr;

        const auto w = sculpt::morphWeights (sculpt::applyOffset (morphSm.getCurrentValue(), slot->rrOffset), p.gamma);
        slot->w[0] = w.natural; slot->w[1] = w.treated; slot->w[2] = w.electronic;
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
        auto& p = pieces[(size_t) v.piece];
        const auto w = sculpt::morphWeights (sculpt::applyOffset (m1, v.rrOffset), p.gamma);
        const float w1[3] = { w.natural, w.treated, w.electronic };

        const float* dl[3] = { nullptr, nullptr, nullptr };
        const float* dr[3] = { nullptr, nullptr, nullptr };
        int len[3] = { 0, 0, 0 };
        for (int k = 0; k < 3; ++k)
            if (v.src[k] != nullptr)
            {
                dl[k] = v.src[k]->getReadPointer (0);
                dr[k] = v.src[k]->getReadPointer (1);
                len[k] = v.src[k]->getNumSamples();
            }

        bool dead = false;
        // pesi interpolati nel blocco: MORPH segue anche i colpi già partiti, senza salti
        for (int i = 0; i < n && ! dead; ++i)
        {
            const float t = (float) i / (float) n;
            const int idx = v.pos + i;
            float sl = 0.0f, sr = 0.0f;
            for (int k = 0; k < 3; ++k)
                if (idx < len[k])
                {
                    const float g = v.w[k] + (w1[k] - v.w[k]) * t;
                    sl += g * dl[k][idx];
                    sr += g * dr[k][idx];
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
        for (int k = 0; k < 3; ++k) v.w[k] = w1[k];
        if (dead || v.pos >= juce::jmax (len[0], len[1], len[2])) v.active = false;
    }

    const auto cw = sculpt::morphWeights (m1, 1.0f);
    layerWeight[0] = cw.natural; layerWeight[1] = cw.treated; layerWeight[2] = cw.electronic;
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
