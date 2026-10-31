// TEAROUT ENGINE - the knowledge base: progressions, genre profiles, layer patterns
#pragma once

#include "Theory.h"
#include "Types.h"

#include <string>
#include <vector>

namespace te
{
// ---- layer patterns (id 0 is always "Off") -----------------------------------------------------
namespace pat
{
enum Sub : int   { SubOff = 0, SubHold, SubHoldKick, SubFollowBass, SubOffbeat8, SubPulse4, SubCount };
enum Bass : int  { BassOff = 0, BassLongHold, BassHoldRestrike, BassRide332, BassRide422, BassPump8,
                   BassOffbeat8, BassTriplet, BassFromSeed, BassCount };
enum BassB : int { BOff = 0, BDouble, BAnswers, BSwap, BRestrikes, BHalfBar, BCount };
enum Chords : int{ ChOff = 0, ChHold, ChStabs332, ChPulse8, ChOffbeat8, ChHalfTimeStabs, ChFromSeed, ChCount };
enum Lead : int  { LeadOff = 0, LeadHook, LeadLong, LeadRiff8, LeadCallResponse, LeadFromSeed, LeadCount };
enum Arp : int   { ArpOff = 0, ArpUp16, ArpDown16, ArpUpDown16, ArpUp2Oct, ArpUp8, ArpUpTrip, ArpPluck332, ArpCount };
enum Drums : int { DrOff = 0, DrHalfTime, DrHalfTimeHeavy, DrTrap, DrFourFloor, DrHardstyle, DrBackbeat, DrCount };
enum Fx : int    { FxOff = 0, FxImpacts, FxImpactsRisers, FxSweeps, FxCount };
} // namespace pat

struct PatternInfo
{
    const char* name;
    const char* help;
    bool usesSeed = false;
};

const std::vector<PatternInfo>& patternsFor (Layer layer);    // index = pattern id
std::vector<std::string> patternChoiceNames (Layer layer);    // "Genre" + pattern names (param choices)
const char* layerName (Layer layer);
const char* layerHelp (Layer layer);
bool layerUsesOctave (Layer layer);
bool layerUsesLength (Layer layer);
float defaultLength (Layer layer);

// ---- progressions ------------------------------------------------------------------------------
struct ProgressionDef
{
    const char* numerals;   // roman numerals, space separated
    Mode mode;
    const char* feel;
};

const std::vector<ProgressionDef>& progressionCatalogue();
// Parameter choices: 0 = Auto (genre pick), 1 = From Seed, 2.. = catalogue entries.
constexpr int kProgressionAuto = 0;
constexpr int kProgressionSeed = 1;
constexpr int kProgressionFirst = 2;
std::vector<std::string> progressionChoiceNames();
std::string prettyNumerals (const char* numerals);   // "i VI III VII" -> "i–VI–III–VII"

// ---- genres ------------------------------------------------------------------------------------
struct Range { int lo, hi; };

struct GenreProfile
{
    const char* name;
    const char* shortName;
    double bpm;
    Voicing voicing;
    bool doubleRootBelow;
    Range chordRange;
    Range leadRange;
    int arpLow;
    int subLow;      // sub/808 register: [subLow, subLow + 11]
    int bassLow;     // mid-bass register
    int defaults[kNumLayers];                 // default pattern id per layer
    std::vector<std::vector<int>> grooveTiers; // re-strike points (ticks within a bar), by priority tier
    std::vector<std::pair<int, double>> picks; // progression catalogue index + weight (Auto)
    std::vector<int> hookTemplates;            // preferred hook rhythm templates
    const char* summary;
};

const GenreProfile& genreProfile (Genre g);
// Weighted patterns a genre uses for a layer (GENERATE picks among these for layers set to "Genre").
const std::vector<std::pair<int, double>>& genrePatternPool (Genre g, Layer l);
std::vector<std::string> genreChoiceNames();
std::vector<std::string> keyChoiceNames();

// ---- rhythm templates for the lead (2 bars, in 1/16 steps) ----------------------------------------
struct Hit { int step, len; };
const std::vector<std::vector<Hit>>& leadTemplates (int leadPattern);
} // namespace te
