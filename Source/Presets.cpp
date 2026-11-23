#include "Presets.h"

namespace ss
{
namespace
{
// Pattern strings: 'x' = 1, '.' = 0, digits '1'..'9' = n/9, spaces and '|' ignored.
LaneSpec& pattern (LaneSpec& l, const juce::String& pat)
{
    int n = 0;
    for (auto c : pat)
    {
        if (n >= kMaxSteps) break;
        if (c == ' ' || c == '|') continue;
        float v = 1.0f;
        if (c == '.') v = 0.0f;
        else if (c >= '1' && c <= '9') v = (float) (c - '0') / 9.0f;
        l.values[(size_t) n++] = v;
    }
    l.length = juce::jmax (1, n);
    return l;
}

LaneSpec& valuesFrom (LaneSpec& l, std::initializer_list<float> vals)
{
    int n = 0;
    for (auto v : vals)
        if (n < kMaxSteps) l.values[(size_t) n++] = v;
    l.length = juce::jmax (1, n);
    return l;
}

LaneSpec& allShapes (LaneSpec& l, int shape) { l.shapes.fill (shape); return l; }

LaneSpec& lane (Preset& p, int module, int rate, float depth = 1.0f, float smooth = 0.2f)
{
    auto& l = p.lanes[(size_t) module];
    l.on = true;
    l.rate = rate;
    l.depth = depth;
    l.smooth = smooth;
    return l;
}

std::vector<int> euclid (int hits, int steps)
{
    std::vector<int> r ((size_t) steps, 0);
    for (int i = 0; i < steps; ++i)
        r[(size_t) i] = ((i * hits) % steps) < hits ? 1 : 0;
    return r;
}

struct Builder
{
    std::vector<Preset> list;
    juce::String category;

    Preset& add (const juce::String& name)
    {
        list.emplace_back();
        list.back().name = name;
        list.back().category = category;
        return list.back();
    }
};

//==============================================================================
void sidechain (Builder& b)
{
    b.category = "Sidechain";

    struct Def { const char* name; int rate; int shape; float depth; float smooth; };
    const Def defs[] {
        { "Classic Pump 1/4",   Rate::Quarter,    Pump,   1.00f, 0.15f },
        { "Soft Pump 1/4",      Rate::Quarter,    Pump,   0.55f, 0.20f },
        { "Tight Duck 1/4",     Rate::Quarter,    Duck,   1.00f, 0.10f },
        { "Linear Pump 1/4",    Rate::Quarter,    RampUp, 1.00f, 0.15f },
        { "Pump 1/8",           Rate::Eighth,     Pump,   1.00f, 0.15f },
        { "Gentle Pump 1/8",    Rate::Eighth,     Pump,   0.45f, 0.20f },
        { "Duck 1/8",           Rate::Eighth,     Duck,   0.90f, 0.10f },
        { "Half-Time Pump",     Rate::Half,       Pump,   1.00f, 0.15f },
        { "Triplet Pump",       Rate::QuarterT,   Pump,   0.90f, 0.15f },
        { "Dotted Pump",        Rate::EighthD,    Pump,   0.85f, 0.15f },
        { "Flutter Pump 1/16",  Rate::Sixteenth,  Pump,   0.60f, 0.15f },
        { "S-Curve Breather",   Rate::Quarter,    SCurve, 0.80f, 0.20f },
    };

    for (auto& d : defs)
    {
        auto& p = b.add (d.name);
        allShapes (pattern (lane (p, Vol, d.rate, d.depth, d.smooth), "xxxx"), d.shape);
    }

    {
        auto& p = b.add ("Kick Pump + Snare Duck");
        auto& l = pattern (lane (p, Vol, Rate::Quarter, 1.0f, 0.15f), "x 6 x 6");
        l.shapes[0] = Pump; l.shapes[1] = Duck; l.shapes[2] = Pump; l.shapes[3] = Duck;
    }
    {
        auto& p = b.add ("Swung Pump 1/8");
        allShapes (pattern (lane (p, Vol, Rate::Eighth, 0.9f, 0.15f), "xxxx"), Pump);
        p.params["swing"] = 0.55f;
    }
    {
        auto& p = b.add ("Pump + Filter Dip");
        allShapes (pattern (lane (p, Vol, Rate::Quarter, 0.8f, 0.15f), "xxxx"), Pump);
        allShapes (pattern (lane (p, Flt, Rate::Quarter, 0.7f, 0.2f), "xxxx"), Pump);
    }
    {
        auto& p = b.add ("Pump + Grit");
        allShapes (pattern (lane (p, Vol, Rate::Quarter, 1.0f, 0.15f), "xxxx"), Pump);
        allShapes (pattern (lane (p, Dst, Rate::Quarter, 1.0f, 0.2f), "xxxx"), ExpDown);
        p.params["dst_drive"] = 20.0f;
        p.params["dst_mix"] = 0.6f;
    }
}

//==============================================================================
void tranceGates (Builder& b)
{
    b.category = "Trance Gate";

    struct Def { const char* name; const char* pat; int rate; };
    const Def defs[] {
        { "Straight Eighths",  "x.x.x.x.x.x.x.x.",                  Rate::Sixteenth },
        { "Three On",          "xxx.xxx.xxx.xxx.",                  Rate::Sixteenth },
        { "Rolling",           "x.xx.xx.x.xx.xx.",                  Rate::Sixteenth },
        { "Skipper",           "xx.xx.x.xx.xx.x.",                  Rate::Sixteenth },
        { "Dotted Pulse",      "x..x..x..x..x.x.",                  Rate::Sixteenth },
        { "Uplifter",          "x.x..x.xx.x..x.x",                  Rate::Sixteenth },
        { "Busy Hands",        "xxxx.x.xx.x.xxx.",                  Rate::Sixteenth },
        { "Sparse",            "x...x.x.x...x.xx",                  Rate::Sixteenth },
        { "Shuffle Gate",      "xx.x.xx.x.xx.x.x",                  Rate::Sixteenth },
        { "Stomp",             "x.xxx.x.x.xxx.xx",                  Rate::Sixteenth },
        { "Pairs",             "xx..xx..xx..x.x.",                  Rate::Sixteenth },
        { "Builder",           "x.x.xx.x.x.x.xxx",                  Rate::Sixteenth },
        { "Two-Bar Story",     "x.x.xx.xx.x.xx.x|x.xx.x.xx.x.x.xx", Rate::Sixteenth },
        { "Two-Bar Answer",    "xx.x.x.xx..x.x.x|x.x.x.x.xxxx.x.x", Rate::Sixteenth },
        { "Triplet Gate",      "x.xx.xx.xx.x",                      Rate::SixteenthT },
        { "Thirty-Second Run", "x.x.x.x.xxxxxxxx",                  Rate::ThirtySecond },
    };

    for (auto& d : defs)
    {
        auto& p = b.add (d.name);
        pattern (lane (p, Vol, d.rate, 1.0f, 0.08f), d.pat);
    }

    // Accented gates use step levels.
    struct Acc { const char* name; const char* pat; };
    const Acc accents[] {
        { "Accent Waves",   "95959595 97959593" },
        { "Ghost Notes",    "x3x3x33x x3x3x3xx" },
        { "Velocity Climb", "1234567x 1234567x" },
        { "Echo Gate",      "x.6.3.x.6.3.x.6." },
    };
    for (auto& a : accents)
    {
        auto& p = b.add (a.name);
        pattern (lane (p, Vol, Rate::Sixteenth, 1.0f, 0.08f), a.pat);
    }

    // Euclidean rhythms.
    const int hits[] { 3, 5, 7, 9, 11, 13 };
    for (auto k : hits)
    {
        auto& p = b.add ("Euclid " + juce::String (k) + "/16");
        auto& l = lane (p, Vol, Rate::Sixteenth, 1.0f, 0.08f);
        auto e = euclid (k, 16);
        for (int i = 0; i < 16; ++i) l.values[(size_t) i] = (float) e[(size_t) i];
        l.length = 16;
    }
}

//==============================================================================
void filters (Builder& b)
{
    b.category = "Filter";
    juce::Random rng (1234);

    auto setFilter = [] (Preset& p, int type, float lo, float hi, float res)
    {
        p.params["flt_type"] = (float) type;
        p.params["flt_min"] = lo;
        p.params["flt_max"] = hi;
        p.params["flt_res"] = res;
    };

    {
        auto& p = b.add ("Staircase Up");
        auto& l = lane (p, Flt, Rate::Sixteenth, 1.0f, 0.1f);
        for (int i = 0; i < 16; ++i) l.values[(size_t) i] = (float) i / 15.0f;
        l.length = 16;
        setFilter (p, 0, 150, 12000, 0.35f);
    }
    {
        auto& p = b.add ("Staircase Down");
        auto& l = lane (p, Flt, Rate::Sixteenth, 1.0f, 0.1f);
        for (int i = 0; i < 16; ++i) l.values[(size_t) i] = 1.0f - (float) i / 15.0f;
        l.length = 16;
        setFilter (p, 0, 150, 12000, 0.35f);
    }
    {
        auto& p = b.add ("HP Riser 2 Bars");
        auto& l = lane (p, Flt, Rate::Sixteenth, 1.0f, 0.2f);
        for (int i = 0; i < 32; ++i) l.values[(size_t) i] = (float) i / 31.0f;
        l.length = 32;
        setFilter (p, 1, 20, 4000, 0.3f);
    }
    {
        auto& p = b.add ("Acid Steps");
        auto& l = lane (p, Flt, Rate::Sixteenth, 1.0f, 0.05f);
        for (int i = 0; i < 16; ++i) { l.values[(size_t) i] = 0.25f + 0.75f * rng.nextFloat(); l.shapes[(size_t) i] = ExpDown; }
        l.length = 16;
        setFilter (p, 0, 120, 6000, 0.8f);
    }
    {
        auto& p = b.add ("Acid Squelch");
        auto& l = pattern (lane (p, Flt, Rate::Sixteenth, 1.0f, 0.05f), "x5x9x3x7 x9x5x7x9");
        allShapes (l, Duck);
        setFilter (p, 0, 100, 5000, 0.85f);
    }

    struct Wob { const char* name; int rate; int shape; };
    const Wob wobs[] {
        { "Wobble 1/4",     Rate::Quarter,    Triangle },
        { "Wobble 1/8",     Rate::Eighth,     Triangle },
        { "Wobble 1/8T",    Rate::EighthT,    Triangle },
        { "Sine Wub 1/4",   Rate::Quarter,    Sine },
        { "Filter Pump",    Rate::Quarter,    Pump },
        { "Slow Sweep",     Rate::Whole,      Sine },
    };
    for (auto& w : wobs)
    {
        auto& p = b.add (w.name);
        allShapes (pattern (lane (p, Flt, w.rate, 1.0f, 0.2f), "xxxx"), w.shape);
        setFilter (p, 0, 100, 8000, 0.55f);
    }
    {
        auto& p = b.add ("Talking Band");
        auto& l = lane (p, Flt, Rate::Eighth, 1.0f, 0.3f);
        for (int i = 0; i < 8; ++i) { l.values[(size_t) i] = rng.nextFloat(); l.shapes[(size_t) i] = SCurve; }
        l.length = 8;
        setFilter (p, 2, 300, 4000, 0.7f);
    }
    {
        auto& p = b.add ("Sample & Hold");
        auto& l = lane (p, Flt, Rate::Sixteenth, 1.0f, 0.05f);
        for (int i = 0; i < 16; ++i) l.values[(size_t) i] = rng.nextFloat();
        l.length = 16;
        setFilter (p, 0, 200, 10000, 0.5f);
    }
    {
        auto& p = b.add ("Smooth S&H");
        auto& l = lane (p, Flt, Rate::Eighth, 1.0f, 0.7f);
        for (int i = 0; i < 16; ++i) l.values[(size_t) i] = rng.nextFloat();
        l.length = 16;
        setFilter (p, 0, 200, 10000, 0.4f);
    }
    {
        auto& p = b.add ("Filter Gate");
        pattern (lane (p, Flt, Rate::Sixteenth, 0.85f, 0.08f), "x.x.xx.x.xx.x.x.");
        setFilter (p, 0, 300, 15000, 0.25f);
    }
    {
        auto& p = b.add ("Arp Filter");
        valuesFrom (lane (p, Flt, Rate::Sixteenth, 1.0f, 0.1f), { 0.2f, 0.5f, 0.8f, 0.5f, 0.3f, 0.6f, 1.0f, 0.6f });
        setFilter (p, 0, 200, 9000, 0.6f);
    }
    {
        auto& p = b.add ("Telephone Chop");
        allShapes (pattern (lane (p, Flt, Rate::Eighth, 1.0f, 0.15f), "x.x5x.95"), HalfGate);
        setFilter (p, 2, 600, 3000, 0.5f);
    }
    {
        auto& p = b.add ("Notch Sweeper");
        allShapes (pattern (lane (p, Flt, Rate::Half, 1.0f, 0.3f), "xx"), Triangle);
        setFilter (p, 3, 200, 8000, 0.4f);
    }
}

//==============================================================================
void distortion (Builder& b)
{
    b.category = "Distortion";

    struct Def { const char* name; int type; float drive; float mix; const char* pat; int rate; int shape; };
    const Def defs[] {
        { "Drive Pulse",       0, 24, 1.0f,  "x.x.x.x.",          Rate::Sixteenth, Flat },
        { "Saturated Pump",    0, 18, 0.8f,  "xxxx",              Rate::Quarter,   ExpDown },
        { "Hard Clip Accents", 1, 26, 1.0f,  "x..x..x.",          Rate::Sixteenth, Flat },
        { "Clip Swell",        1, 30, 0.9f,  "xx",                Rate::Half,      Swell },
        { "Wavefold Steps",    2, 20, 1.0f,  "2468x864",          Rate::Sixteenth, Flat },
        { "Fold Wobble",       2, 24, 0.8f,  "xxxx",              Rate::Eighth,    Triangle },
        { "Bitcrush Glitch",   3, 34, 1.0f,  "x..x.x...x.xx...",  Rate::Sixteenth, Flat },
        { "Crush Ramp",        3, 36, 1.0f,  "xx",                Rate::Half,      RampUp },
        { "Lo-Fi Stutter",     3, 28, 0.9f,  "....x.......x.x.",  Rate::Sixteenth, Stutter },
        { "Tape Saturation",   0, 12, 0.7f,  "x6x6",              Rate::Eighth,    Sine },
    };
    for (auto& d : defs)
    {
        auto& p = b.add (d.name);
        allShapes (pattern (lane (p, Dst, d.rate, 1.0f, 0.15f), d.pat), d.shape);
        p.params["dst_type"] = (float) d.type;
        p.params["dst_drive"] = d.drive;
        p.params["dst_mix"] = d.mix;
    }
    {
        auto& p = b.add ("Dirty Gate");
        pattern (lane (p, Vol, Rate::Sixteenth, 1.0f, 0.08f), "x.xx.xx.x.xx.xx.");
        pattern (lane (p, Dst, Rate::Sixteenth, 1.0f, 0.1f), "x...x...x...x.x.");
        p.params["dst_type"] = 1; p.params["dst_drive"] = 22; p.params["dst_mix"] = 1.0f;
    }
    {
        auto& p = b.add ("Crushed Filter Arp");
        valuesFrom (lane (p, Flt, Rate::Sixteenth, 1.0f, 0.1f), { 0.3f, 0.7f, 1.0f, 0.5f, 0.2f, 0.8f, 0.6f, 0.9f });
        pattern (lane (p, Dst, Rate::Sixteenth, 0.8f, 0.1f), "x..x..x.");
        p.params["dst_type"] = 3; p.params["dst_drive"] = 30; p.params["dst_mix"] = 0.8f;
        p.params["flt_res"] = 0.6f;
    }
}

//==============================================================================
void phaseFlange (Builder& b)
{
    b.category = "Phaser & Flanger";

    struct PDef { const char* name; const char* pat; int rate; int shape; float fb; float mix; };
    const PDef phasers[] {
        { "Phase Sweep 1 Bar",  "x",               Rate::Whole,     Triangle, 0.5f, 1.0f },
        { "Phase Steps",        "13579x75",        Rate::Sixteenth, Flat,     0.6f, 1.0f },
        { "Phase Pump",         "xxxx",            Rate::Quarter,   Pump,     0.4f, 0.9f },
        { "Resonant Phaser",    "xx",              Rate::Half,      Sine,     0.85f, 1.0f },
        { "Phase Jumps",        "x3x..7x2",        Rate::Eighth,    Flat,     0.6f, 1.0f },
        { "Swirl 1/8T",         "xxx",             Rate::EighthT,   Triangle, 0.6f, 0.8f },
    };
    for (auto& d : phasers)
    {
        auto& p = b.add (d.name);
        allShapes (pattern (lane (p, Phs, d.rate, 1.0f, 0.3f), d.pat), d.shape);
        p.params["phs_fb"] = d.fb;
        p.params["phs_mix"] = d.mix;
    }

    struct FDef { const char* name; const char* pat; int rate; int shape; float range; float fb; float mix; };
    const FDef flangers[] {
        { "Jet Flange",         "x",        Rate::Whole,     Triangle, 6.0f, 0.7f,  0.8f },
        { "Flange Steps",       "2468x642", Rate::Sixteenth, Flat,     4.0f, 0.6f,  0.7f },
        { "Metallic Pump",      "xxxx",     Rate::Quarter,   Pump,     3.0f, 0.85f, 0.7f },
        { "Negative Flange",    "xx",       Rate::Half,      Sine,     5.0f, -0.7f, 0.8f },
        { "Comb Arp",           "x5x2x7x3", Rate::Sixteenth, Flat,     2.0f, 0.9f,  0.6f },
        { "Through-Zero Swell", "x",        Rate::Whole,     Swell,    8.0f, 0.5f,  0.9f },
    };
    for (auto& d : flangers)
    {
        auto& p = b.add (d.name);
        allShapes (pattern (lane (p, Flg, d.rate, 1.0f, 0.4f), d.pat), d.shape);
        p.params["flg_range"] = d.range;
        p.params["flg_fb"] = d.fb;
        p.params["flg_mix"] = d.mix;
    }
}

//==============================================================================
void pitch (Builder& b)
{
    b.category = "Pitch";

    struct Def { const char* name; const char* pat; int rate; int shape; float range; float smooth; float mix; };
    const Def defs[] {
        { "Tape Stop 1/2",    "xx",               Rate::Half,      RampUp,   25.0f, 0.4f, 1.0f },
        { "Tape Start 1/2",   "xx",               Rate::Half,      RampDown, 25.0f, 0.4f, 1.0f },
        { "Warble",           "xxxx",             Rate::Eighth,    Sine,     4.0f,  0.5f, 1.0f },
        { "Vinyl Wow",        "x",                Rate::Whole,     Sine,     8.0f,  0.6f, 1.0f },
        { "Chorus Steps",     "x6x6",             Rate::Quarter,   Triangle, 3.0f,  0.6f, 0.5f },
        { "Pitch Dive 1/4",   "xxxx",             Rate::Quarter,   Swell,    15.0f, 0.4f, 1.0f },
        { "Glitch Bend",      "x..x.x..x...x.x.", Rate::Sixteenth, RampUp,   10.0f, 0.4f, 1.0f },
        { "Seasick",          "xx",               Rate::HalfT,     Sine,     12.0f, 0.6f, 1.0f },
        { "Detune Wobble",    "x5x5",             Rate::Eighth,    Triangle, 2.0f,  0.5f, 0.5f },
        { "Bounce Bend",      "xx",               Rate::Quarter,   Bounce,   6.0f,  0.5f, 1.0f },
    };
    for (auto& d : defs)
    {
        auto& p = b.add (d.name);
        allShapes (pattern (lane (p, Pit, d.rate, 1.0f, d.smooth), d.pat), d.shape);
        p.params["pit_range"] = d.range;
        p.params["pit_mix"] = d.mix;
    }
}

//==============================================================================
void delays (Builder& b)
{
    b.category = "Delay Throws";

    struct Def { const char* name; const char* pat; int rate; int time; float fb; float mix; bool gate; };
    const Def defs[] {
        { "Throw On 4",         "...x",             Rate::Quarter,   Rate::EighthD,   0.45f, 0.7f, false },
        { "End-Of-Bar Throw",   "...............x", Rate::Sixteenth, Rate::Eighth,    0.55f, 0.7f, false },
        { "Dotted Echo Steps",  "x.x.x...",         Rate::Sixteenth, Rate::EighthD,   0.40f, 0.6f, false },
        { "Ping Stutter",       "x..x..x.",         Rate::Sixteenth, Rate::Sixteenth, 0.65f, 0.5f, false },
        { "Triplet Echoes",     "x.x",              Rate::EighthT,   Rate::QuarterT,  0.50f, 0.6f, false },
        { "Long Tail Throw",    "x.......",         Rate::Eighth,    Rate::Quarter,   0.75f, 0.6f, false },
        { "Gated Echo",         "x.xx.xx.x.xx.xx.", Rate::Sixteenth, Rate::EighthD,   0.50f, 0.6f, true },
        { "Echo Swell",         "x",                Rate::Whole,     Rate::Eighth,    0.60f, 0.7f, false },
        { "Slapback Steps",     "x.x.x5x.",         Rate::Sixteenth, Rate::ThirtySecond, 0.2f, 0.5f, false },
        { "Dub Throws",         "....x.......x...", Rate::Sixteenth, Rate::QuarterD,  0.70f, 0.7f, false },
    };
    for (auto& d : defs)
    {
        auto& p = b.add (d.name);
        auto& l = pattern (lane (p, Dly, d.rate, 1.0f, 0.1f), d.pat);
        if (juce::String (d.name) == "Echo Swell") allShapes (l, Swell);
        p.params["dly_time"] = (float) d.time;
        p.params["dly_fb"] = d.fb;
        p.params["dly_mix"] = d.mix;
        if (d.gate)
            pattern (lane (p, Vol, Rate::Sixteenth, 1.0f, 0.08f), d.pat);
    }
}

//==============================================================================
void tremoloStutter (Builder& b)
{
    b.category = "Tremolo & Stutter";

    struct Def { const char* name; const char* pat; int rate; int shape; float depth; };
    const Def defs[] {
        { "Tremolo 1/8",       "xxxx",             Rate::Eighth,     Sine,     0.6f },
        { "Tremolo 1/16",      "xxxx",             Rate::Sixteenth,  Sine,     0.5f },
        { "Triplet Tremolo",   "xxx",              Rate::EighthT,    Triangle, 0.7f },
        { "Helicopter",        "xxxx",             Rate::ThirtySecond, Triangle, 1.0f },
        { "Chop 1/8",          "xxxx",             Rate::Eighth,     RampDown, 1.0f },
        { "Half Gates",        "xxxx",             Rate::Eighth,     HalfGate, 1.0f },
        { "Stutter Fill",      "xxxxxxxxxxxx....", Rate::Sixteenth,  Flat,     1.0f },
        { "Reverse Swells",    "xxxx",             Rate::Quarter,    Swell,    1.0f },
        { "Bouncing Ball",     "xx",               Rate::Half,       Bounce,   1.0f },
        { "Stair Gate",        "xxxx",             Rate::Quarter,    Stairs,   1.0f },
    };
    for (auto& d : defs)
    {
        auto& p = b.add (d.name);
        allShapes (pattern (lane (p, Vol, d.rate, d.depth, 0.1f), d.pat), d.shape);
    }
    {
        auto& p = b.add ("Stutter End");
        auto& l = pattern (lane (p, Vol, Rate::Sixteenth, 1.0f, 0.05f), "xxxxxxxxxxxxxxxx");
        l.shapes[12] = l.shapes[13] = l.shapes[14] = l.shapes[15] = Stutter;
    }
    {
        auto& p = b.add ("Glitch Stutter");
        auto& l = pattern (lane (p, Vol, Rate::Eighth, 1.0f, 0.05f), "xxxxxxxx");
        l.shapes[3] = Stutter; l.shapes[6] = Stutter; l.shapes[7] = HalfGate;
    }
}

//==============================================================================
void polyrhythm (Builder& b)
{
    b.category = "Polyrhythm";

    struct Def { const char* name; const char* volPat; int fltLen; };
    const Def defs[] {
        { "3 Against 16",      "x..",     16 },
        { "5 Against 16",      "x.x..",   16 },
        { "7 Against 16",      "x.xx.x.", 16 },
        { "3 Against 4",       "xx.",     4 },
        { "5 Over 3",          "x.xx.",   3 },
        { "Drifting Accents",  "x55x5",   12 },
    };
    for (auto& d : defs)
    {
        auto& p = b.add (d.name);
        pattern (lane (p, Vol, Rate::Sixteenth, 1.0f, 0.08f), d.volPat);
        auto& f = lane (p, Flt, Rate::Sixteenth, 0.9f, 0.2f);
        for (int i = 0; i < d.fltLen; ++i) f.values[(size_t) i] = (float) (i + 1) / (float) d.fltLen;
        f.length = d.fltLen;
        p.params["flt_res"] = 0.45f;
    }
}

//==============================================================================
void motion (Builder& b)
{
    b.category = "Motion";
    juce::Random rng (777);

    {
        auto& p = b.add ("Future Bass Chords");
        allShapes (pattern (lane (p, Vol, Rate::Quarter, 0.9f, 0.15f), "xxxx"), Pump);
        allShapes (pattern (lane (p, Flt, Rate::Eighth, 0.6f, 0.3f), "xxxx"), Sine);
        allShapes (pattern (lane (p, Pit, Rate::Whole, 1.0f, 0.6f), "x"), Sine);
        p.params["pit_range"] = 3.0f; p.params["pit_mix"] = 1.0f;
    }
    {
        auto& p = b.add ("Trance Supersaw");
        pattern (lane (p, Vol, Rate::Sixteenth, 1.0f, 0.08f), "x.xx.xx.x.xx.xx.");
        auto& f = lane (p, Flt, Rate::Sixteenth, 0.8f, 0.2f);
        for (int i = 0; i < 32; ++i) f.values[(size_t) i] = (float) i / 31.0f;
        f.length = 32;
        allShapes (pattern (lane (p, Dly, Rate::Quarter, 1.0f, 0.1f), "...x"), Flat);
        p.params["dly_mix"] = 0.4f;
    }
    {
        auto& p = b.add ("Broken Machine");
        pattern (lane (p, Vol, Rate::Sixteenth, 1.0f, 0.05f), "x.x..xx.x...x.xx");
        pattern (lane (p, Dst, Rate::Sixteenth, 1.0f, 0.05f), "...x.....x....x.");
        allShapes (pattern (lane (p, Pit, Rate::Eighth, 1.0f, 0.3f), "x..x..x."), RampUp);
        p.params["dst_type"] = 3; p.params["dst_drive"] = 32;
        p.params["pit_range"] = 8.0f;
    }
    {
        auto& p = b.add ("Liquid Pad");
        allShapes (pattern (lane (p, Flt, Rate::Half, 0.7f, 0.5f), "xx"), Sine);
        allShapes (pattern (lane (p, Phs, Rate::Whole, 1.0f, 0.5f), "x"), Triangle);
        allShapes (pattern (lane (p, Pit, Rate::Whole, 1.0f, 0.7f), "x"), Sine);
        p.params["pit_range"] = 2.0f; p.params["pit_mix"] = 0.5f;
        p.params["phs_mix"] = 0.6f;
    }
    {
        auto& p = b.add ("Garage Chop");
        auto& l = pattern (lane (p, Vol, Rate::Sixteenth, 1.0f, 0.06f), "x.xx..x.x.x..xx.");
        for (int i = 0; i < 16; ++i) l.shapes[(size_t) i] = (i % 3 == 0) ? RampDown : Flat;
        p.params["swing"] = 0.45f;
    }
    {
        auto& p = b.add ("Jet Pump");
        allShapes (pattern (lane (p, Vol, Rate::Quarter, 0.8f, 0.15f), "xxxx"), Pump);
        allShapes (pattern (lane (p, Flg, Rate::Whole, 1.0f, 0.4f), "x"), Triangle);
        p.params["flg_mix"] = 0.6f;
    }
    {
        auto& p = b.add ("Dub Space");
        pattern (lane (p, Vol, Rate::Eighth, 0.6f, 0.15f), "x.x.x.x.");
        pattern (lane (p, Dly, Rate::Sixteenth, 1.0f, 0.1f), "....x.......x.x.");
        allShapes (pattern (lane (p, Flt, Rate::Whole, 0.8f, 0.6f), "x"), Sine);
        p.params["dly_time"] = (float) Rate::QuarterD; p.params["dly_fb"] = 0.7f; p.params["dly_mix"] = 0.6f;
    }
    {
        auto& p = b.add ("Bass Wobble Grit");
        allShapes (pattern (lane (p, Flt, Rate::Eighth, 1.0f, 0.2f), "xxxx"), Triangle);
        allShapes (pattern (lane (p, Dst, Rate::Eighth, 0.8f, 0.2f), "xxxx"), Triangle);
        p.params["flt_res"] = 0.6f; p.params["dst_type"] = 2; p.params["dst_drive"] = 18;
    }
    {
        auto& p = b.add ("Random Motion");
        auto& v = lane (p, Vol, Rate::Sixteenth, 0.7f, 0.1f);
        auto& f = lane (p, Flt, Rate::Sixteenth, 0.8f, 0.2f);
        for (int i = 0; i < 16; ++i)
        {
            v.values[(size_t) i] = rng.nextFloat() < 0.7f ? 1.0f : 0.0f;
            f.values[(size_t) i] = rng.nextFloat();
            v.shapes[(size_t) i] = rng.nextInt (8);
        }
        v.length = f.length = 16;
    }
    {
        auto& p = b.add ("Vocal Chop Machine");
        auto& l = pattern (lane (p, Vol, Rate::Sixteenth, 1.0f, 0.05f), "xx.x.xx.x.x.xx.x");
        l.shapes[4] = Stutter; l.shapes[10] = Stutter;
        pattern (lane (p, Pit, Rate::Sixteenth, 1.0f, 0.4f), "......x.......x.");
        allShapes (p.lanes[Pit], RampUp);
        p.params["pit_range"] = 10.0f;
    }
    {
        auto& p = b.add ("Phased Gate");
        pattern (lane (p, Vol, Rate::Sixteenth, 1.0f, 0.08f), "x.x.xx.x.x.x.xxx");
        allShapes (pattern (lane (p, Phs, Rate::Whole, 1.0f, 0.4f), "xx"), Triangle);
        p.lanes[Phs].rate = Rate::Whole;
    }
    {
        auto& p = b.add ("Lo-Fi Drift");
        allShapes (pattern (lane (p, Pit, Rate::Whole, 1.0f, 0.7f), "x"), Sine);
        allShapes (pattern (lane (p, Flt, Rate::Half, 0.4f, 0.6f), "xx"), Sine);
        pattern (lane (p, Dst, Rate::Eighth, 0.5f, 0.3f), "x6x6x6x6");
        p.params["pit_range"] = 5.0f; p.params["dst_type"] = 3; p.params["dst_drive"] = 24; p.params["dst_mix"] = 0.5f;
        p.params["flt_max"] = 7000;
    }
    {
        auto& p = b.add ("Build-Up 2 Bars");
        auto& v = lane (p, Vol, Rate::Eighth, 1.0f, 0.1f);
        for (int i = 0; i < 16; ++i) v.shapes[(size_t) i] = i < 8 ? Flat : (i < 12 ? HalfGate : Stutter);
        v.length = 16;
        auto& f = lane (p, Flt, Rate::Sixteenth, 1.0f, 0.3f);
        for (int i = 0; i < 32; ++i) f.values[(size_t) i] = (float) i / 31.0f;
        f.length = 32;
        p.params["flt_type"] = 1; p.params["flt_min"] = 20; p.params["flt_max"] = 3000;
    }
    {
        auto& p = b.add ("Everything Moves");
        allShapes (pattern (lane (p, Vol, Rate::Quarter, 0.6f, 0.15f), "xxxx"), Pump);
        allShapes (pattern (lane (p, Flt, Rate::Eighth, 0.5f, 0.3f), "xxxx"), Sine);
        pattern (lane (p, Dst, Rate::Sixteenth, 0.6f, 0.1f), "x...x...x...x.x.");
        allShapes (pattern (lane (p, Phs, Rate::Whole, 1.0f, 0.5f), "x"), Triangle);
        pattern (lane (p, Dly, Rate::Sixteenth, 1.0f, 0.1f), "...............x");
        p.params["dst_drive"] = 14; p.params["phs_mix"] = 0.5f; p.params["dly_mix"] = 0.5f;
    }
}

std::vector<Preset> build()
{
    Builder b;
    b.category = "Init";
    {
        auto& p = b.add ("Init");
        pattern (lane (p, Vol, Rate::Sixteenth, 1.0f, 0.1f), "x.x.x.x.x.x.x.x.");
    }
    sidechain (b);
    tranceGates (b);
    filters (b);
    distortion (b);
    phaseFlange (b);
    pitch (b);
    delays (b);
    tremoloStutter (b);
    polyrhythm (b);
    motion (b);
    return std::move (b.list);
}
} // namespace

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets = build();
    return presets;
}

} // namespace ss
