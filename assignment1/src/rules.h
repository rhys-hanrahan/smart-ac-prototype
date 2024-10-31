#ifndef RULES_H
#define RULES_H

#include <ArduinoJson.h>
#include <vector>
#include <SPIFFS.h>
#include <string>

// Define the structure for AC state tracking
struct ACState {
  bool is_on;
  float current_temp;
  String mode; // e.g., "cool", "heat", "energy_saver"
  bool fan_on;
};

// Define structures for rules
struct Timeframe {
  std::vector<String> days; // Array of days this rule applies (e.g., "Monday")
  String start_time;   // Start time (e.g., "21:00")
  String end_time;     // End time (e.g., "10:00")
  std::vector<String> seasons;   // Seasons this rule applies (e.g., "Winter")
};

struct Condition {
  String field;        // Field to evaluate (e.g., "temperature", "feels_like")
  String operator_;    // Operator (e.g., ">", "<=", "within")
  float value;         // Threshold value for comparison
  String start;        // For range-based conditions (start range)
  String end;          // For range-based conditions (end range)
};

struct Action {
  String type;         // Action type (e.g., "set_temp", "increment_temp")
  float target_temp;   // Target temperature for set_temp action
  float increment_value; // Value to increment or decrement temp by
  Condition repeat_if; // Condition to repeat the action
  Condition condition; // Additional condition for action
};

struct RuleSet {
  String name;                  // Name of the rule set
  String description;           // Description of the rule
  Timeframe timeframe;          // Timeframe when this rule is active
  std::vector<Condition> conditions; // Conditions to evaluate
  std::vector<Action> actions;       // Actions to take if conditions met
};

// Global instances for rules and AC state
extern std::vector<RuleSet> rules;
extern ACState ac_state;

// Time functions
String determineHemisphere();
String getCurrentDay();
String getCurrentTime();
String getCurrentSeason();
int getUTCOffset(int month, int day);
bool isDSTActive(int month, int day);

// Functions to manage rules and AC state
void loadRules();
void saveRules();
bool isTimeframeValid(const Timeframe& timeframe);
bool areConditionsMet(const std::vector<Condition>& conditions);
void executeAction(const Action& action);
void evaluateRules(); // Main function to evaluate all rules against current AC state

#endif // RULES_H