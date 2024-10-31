#include "rules.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ctime>

std::vector<RuleSet> rules;
ACState ac_state = {false, 22.0, "cool", 1 }; // Default state
TemperatureData temperature_data = { 0 };

// Helper to get the current day as a string
String getCurrentDay() {
    time_t now = time(nullptr);
    struct tm *timeInfo = localtime(&now);
    char dayOfWeek[10];
    strftime(dayOfWeek, sizeof(dayOfWeek), "%A", timeInfo); // Full day name (e.g., "Monday")
    return String(dayOfWeek);
}

// Helper to get the current time in HH:MM format
String getCurrentTime() {
    time_t now = time(nullptr);
    struct tm *timeInfo = localtime(&now);
    char currentTime[6];
    strftime(currentTime, sizeof(currentTime), "%H:%M", timeInfo); // Format to HH:MM
    return String(currentTime);
}

int getUTCOffset(int month, int day) {
    struct tm timeinfo;
    time_t rawtime;

    // Set the desired date and time to the beginning of the specified day
    timeinfo.tm_year = 2024 - 1900; // Use a leap year to avoid complications
    timeinfo.tm_mon = month - 1;    // tm_mon is 0-based (0 = January)
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = -1;         // Let the system determine if DST is in effect

    rawtime = mktime(&timeinfo);    // Convert to time_t
    struct tm *localTime = localtime(&rawtime); // Get local time info
    struct tm *utcTime = gmtime(&rawtime);   // UTC time


    // Calculate the offset in seconds
    int utcOffset = (mktime(localTime) - mktime(utcTime));
    return utcOffset; // UTC offset in seconds
}


bool isDSTActive(int month, int day) {
    struct tm timeinfo;
    time_t rawtime;

    // Set the desired date and time to the beginning of the specified day
    timeinfo.tm_year = 2024 - 1900; // Use a leap year to avoid complications
    timeinfo.tm_mon = month - 1;    // tm_mon is 0-based (0 = January)
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = -1;         // Let the system determine if DST is in effect

    rawtime = mktime(&timeinfo);
    struct tm* localTime = localtime(&rawtime);

    // tm_isdst > 0 indicates DST is active
    return localTime->tm_isdst > 0;
}

// Use daylight savings to determine what hemisphere
// the user is in - based on:
// https://stackoverflow.com/a/65658594
String determineHemisphere() {
    int janOffset = getUTCOffset(1, 1);   // January 1st UTC offset
    int julOffset = getUTCOffset(7, 1);   // July 1st UTC offset
    bool janDST = isDSTActive(1, 1);      // Check if DST is active in January
    bool julDST = isDSTActive(7, 1);      // Check if DST is active in July

    // Determine hemisphere based on the offset difference
    int diff = janOffset - julOffset;
    if (diff > 0 || (julDST && !janDST)) return "northern";
    if (diff < 0 || (janDST && !julDST)) return "southern";
    return "unknown"; // For regions near the equator or regions without DST
}

// Helper to get the current season based on month in AU
// EDIT: Going to roughly guess based on UTC offset/DST
// https://stackoverflow.com/a/65658594
String getCurrentSeason() {
    String hemisphere = determineHemisphere();
    time_t now = time(nullptr);
    struct tm *timeInfo = localtime(&now);
    int month = timeInfo->tm_mon + 1;

    // Adjust based on hemisphere, default to Southern (Australia) if unknown
    if (hemisphere == "northern") {
        if (month >= 3 && month <= 5) return "spring";
        if (month >= 6 && month <= 8) return "summer";
        if (month >= 9 && month <= 11) return "autumn";
        return "winter"; // December to February
    } else {
        // Southern Hemisphere or default (Australia)
        if (month >= 3 && month <= 5) return "autumn";
        if (month >= 6 && month <= 8) return "winter";
        if (month >= 9 && month <= 11) return "spring";
        return "summer"; // December to February
    }
}


// Helper function to load rules from SPIFFS
void loadRules() {
    File file = SPIFFS.open("/rules.json", "r");
    if (!file) {
        Serial.println("Failed to open rules file");
        return;
    }

    DynamicJsonDocument doc(4096); // Adjust size as needed
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
        Serial.println("Failed to parse rules file");
        return;
    }

    rules.clear();
    for (JsonObject ruleObj : doc.as<JsonArray>()) {
        RuleSet rule;
        rule.name = ruleObj["name"].as<String>();
        rule.description = ruleObj["description"].as<String>();
        Serial.printf("Loading rule: %s\n", rule.name.c_str());

        // Load timeframe
        JsonObject timeframe = ruleObj["timeframe"];
        for (JsonVariant day : timeframe["days"].as<JsonArray>()) {
            rule.timeframe.days.push_back(day.as<String>());
        }
        rule.timeframe.start_time = timeframe["start_time"].as<String>();
        rule.timeframe.end_time = timeframe["end_time"].as<String>();
        for (JsonVariant season : timeframe["seasons"].as<JsonArray>()) {
            rule.timeframe.seasons.push_back(season.as<String>());
        }

        // Load main condition group
        JsonObject conditionsObj = ruleObj["conditions"];
        rule.conditions = loadConditionGroup(conditionsObj);

        // Load actions
        for (JsonObject actionObj : ruleObj["actions"].as<JsonArray>()) {
            Action action;
            action.type = actionObj["type"].as<String>();
            action.target_temp = actionObj["target_temp"].as<float>();
            action.increment_value = actionObj["increment_value"].as<float>();

            // Load repeat_if condition
            JsonObject repeatIfObj = actionObj["repeat_if"];
            if (!repeatIfObj.isNull()) {
                action.repeat_if.field = repeatIfObj["field"].as<String>();
                action.repeat_if.operator_ = repeatIfObj["operator"].as<String>();
                action.repeat_if.value = repeatIfObj["value"].as<float>();
            }

            // Load additional condition group for action
            JsonObject conditionGroupObj = actionObj["condition"];
            if (!conditionGroupObj.isNull()) {
                action.condition = loadConditionGroup(conditionGroupObj);
            }

            rule.actions.push_back(action);
        }

        Serial.printf("Rule loaded: %s\n", rule.name.c_str());
        Serial.printf("Conditions: %d, Actions: %d\n", rule.conditions.conditions.size(), rule.actions.size());
        Serial.println("----");
        rules.push_back(rule);
    }
    Serial.println("Rules loaded successfully");
    Serial.printf("Loaded %d rules\n", rules.size());
}

// Helper function to save rules to SPIFFS
void saveRules() {
    File file = SPIFFS.open("/rules.json", "w");
    if (!file) {
        Serial.println("Failed to open rules file for writing");
        return;
    }

    DynamicJsonDocument doc(4096); // Adjust size as needed
    JsonArray rulesArray = doc.to<JsonArray>();

    for (const RuleSet &rule : rules) {
        JsonObject ruleObj = rulesArray.createNestedObject();
        ruleObj["name"] = rule.name;
        ruleObj["description"] = rule.description;

        JsonObject timeframe = ruleObj.createNestedObject("timeframe");
        JsonArray days = timeframe.createNestedArray("days");
        for (const String &day : rule.timeframe.days) days.add(day);
        timeframe["start_time"] = rule.timeframe.start_time;
        timeframe["end_time"] = rule.timeframe.end_time;
        JsonArray seasons = timeframe.createNestedArray("seasons");
        for (const String &season : rule.timeframe.seasons) seasons.add(season);

        // Save main condition group
        JsonObject conditionsObj = ruleObj.createNestedObject("conditions");
        saveConditionGroup(rule.conditions, conditionsObj);

        // Save actions
        JsonArray actions = ruleObj.createNestedArray("actions");
        for (const Action &action : rule.actions) {
            JsonObject actionObj = actions.createNestedObject();
            actionObj["type"] = action.type;
            actionObj["target_temp"] = action.target_temp;
            actionObj["increment_value"] = action.increment_value;

            // Save repeat_if condition
            JsonObject repeatIfObj = actionObj.createNestedObject("repeat_if");
            repeatIfObj["field"] = action.repeat_if.field;
            repeatIfObj["operator"] = action.repeat_if.operator_;
            repeatIfObj["value"] = action.repeat_if.value;

            // Save additional condition group for action
            JsonObject conditionGroupObj = actionObj.createNestedObject("condition");
            saveConditionGroup(action.condition, conditionGroupObj);
        }
    }

    serializeJson(doc, file);
    file.close();
    Serial.println("Rules saved successfully");
    Serial.printf("Saved %d rules\n", rules.size());
}

// Helper to load a ConditionGroup from JSON
ConditionGroup loadConditionGroup(JsonObject &groupObj) {
    ConditionGroup group;
    group.operator_ = groupObj["operator"].as<String>();

    // Load each condition or nested group
    for (JsonVariant condition : groupObj["conditions"].as<JsonArray>()) {
        if (condition.is<JsonObject>()) {
            JsonObject conditionObj = condition.as<JsonObject>();

            // Check if it’s a nested group or a simple condition
            if (conditionObj.containsKey("operator")) {
                ConditionGroup nestedGroup = loadConditionGroup(conditionObj);
                group.groups.push_back(nestedGroup); // Add as nested group
            } else {
                Condition simpleCondition;
                simpleCondition.field = conditionObj["field"].as<String>();
                simpleCondition.operator_ = conditionObj["operator"].as<String>();
                simpleCondition.value = conditionObj["value"].as<float>();
                group.conditions.push_back(simpleCondition); // Add as simple condition
            }
        }
    }
    return group;
}

// Helper to save a ConditionGroup to JSON
void saveConditionGroup(const ConditionGroup &group, JsonObject &groupObj) {
    groupObj["operator"] = group.operator_;
    JsonArray conditions = groupObj.createNestedArray("conditions");

    for (const Condition &condition : group.conditions) {
        JsonObject conditionObj = conditions.createNestedObject();
        conditionObj["field"] = condition.field;
        conditionObj["operator"] = condition.operator_;
        conditionObj["value"] = condition.value;
    }

    for (const ConditionGroup &nestedGroup : group.groups) {
        JsonObject nestedGroupObj = conditions.createNestedObject();
        saveConditionGroup(nestedGroup, nestedGroupObj);
    }
}


// Function to check if the current time is within a rule's timeframe
// Works off local time only.
bool isTimeframeValid(const Timeframe &timeframe) {
    // Get the current day, time, and season
    String currentDay = getCurrentDay();
    String currentTime = getCurrentTime();
    String currentSeason = getCurrentSeason();

    // Check if the current day is within the allowed days
    if (!timeframe.days.empty() && std::find(timeframe.days.begin(), timeframe.days.end(), currentDay) == timeframe.days.end()) {
        return false;
    }

    // Check if the current season is within the allowed seasons
    if (!timeframe.seasons.empty() && std::find(timeframe.seasons.begin(), timeframe.seasons.end(), currentSeason) == timeframe.seasons.end()) {
        return false;
    }

    // Check if the current time falls within the start and end times
    if (timeframe.start_time <= currentTime && currentTime <= timeframe.end_time) {
        return true;
    }

    return false;
}

// Function to check if all conditions in a rule are met
bool areConditionsMet(const ConditionGroup &conditionGroup, const ACState &ac_state) {
    bool result = (conditionGroup.operator_ == "AND");

    for (const Condition &condition : conditionGroup.conditions) {
        bool conditionMet = false;
        float value = 0.0;

        // Determine value based on the condition field
        if (condition.field == "temperature") {
            value = temperature_data.temperature;
        } else if (condition.field == "feels_like_temp") {
            value = temperature_data.feels_like; // Replace with actual calculation if available
        } else if (condition.field == "time_of_day") {
            conditionMet = (currentTimeIsWithinRange(condition.start, condition.end));
        }

        // Evaluate the condition operator
        if (condition.operator_ == ">" && value > condition.value) conditionMet = true;
        else if (condition.operator_ == "<" && value < condition.value) conditionMet = true;
        else if (condition.operator_ == ">=" && value >= condition.value) conditionMet = true;
        else if (condition.operator_ == "<=" && value <= condition.value) conditionMet = true;
        else if (condition.operator_ == "between") conditionMet = (value >= condition.start.toFloat() && value <= condition.end.toFloat());

        // Combine with previous results based on the operator
        if (conditionGroup.operator_ == "AND") result = result && conditionMet;
        else if (conditionGroup.operator_ == "OR") result = result || conditionMet;

        // Exit early if result is already determined
        if (result == (conditionGroup.operator_ == "OR")) return result;
    }
    return result;
}

bool currentTimeIsWithinRange(String start, String end) {
    // Compare current time with start and end times
    // Convert times to comparable format and evaluate
    time_t now = time(nullptr);
    struct tm *timeInfo = localtime(&now);
    String currentTimeStr = String(timeInfo->tm_hour) + ":" + String(timeInfo->tm_min);
    
    // Implement comparison logic here
    return (currentTimeStr >= start && currentTimeStr <= end);
}

// Function to execute actions based on the rule's configuration
void executeAction(const Action &action, ACState &ac_state) {
    if (action.type == "set_temp") {
        ac_state.current_temp = action.target_temp;
        Serial.printf("AC temperature set to %.2f\n", ac_state.current_temp);
    } else if (action.type == "increment_temp") {
        ac_state.current_temp += action.increment_value; // Might be negative for decrement
        if (action.increment_value < 0) {
            Serial.printf("AC temperature decremented by %.2f to %.2f\n", -action.increment_value, ac_state.current_temp);
        } else {
            Serial.printf("AC temperature incremented by %.2f to %.2f\n", action.increment_value, ac_state.current_temp);
        }
    }
}

// Main function to evaluate all rules and execute actions if conditions are met
void evaluateRules() {
    for (const RuleSet &rule : rules) {
        if (isTimeframeValid(rule.timeframe) && isSeasonValid(rule.timeframe.seasons) && areConditionsMet(rule.conditions, ac_state)) {
            Serial.printf("Executing actions for rule: %s\n", rule.name.c_str());
            for (const Action &action : rule.actions) {
                executeAction(action, ac_state);
            }
        }
    }
}

bool isSeasonValid(const std::vector<String> &seasons) {
    String currentSeason = getCurrentSeason();
    for (const String &season : seasons) {
        if (season == currentSeason) return true;
    }
    return false;
}

bool isDayValid(const std::vector<String> &days) {
    time_t now = time(nullptr);
    struct tm *timeInfo = localtime(&now);
    String currentDay = weekdayToString(timeInfo->tm_wday);
    for (const String &day : days) {
        if (day == currentDay) return true;
    }
    return false;
}

String weekdayToString(int wday) {
    switch (wday) {
        case 0: return "Sunday";
        case 1: return "Monday";
        case 2: return "Tuesday";
        case 3: return "Wednesday";
        case 4: return "Thursday";
        case 5: return "Friday";
        case 6: return "Saturday";
        default: return "";
    }
}

//Based on : https://byjus.com/heat-index-formula
// https://en.wikipedia.org/wiki/Heat_index
// HI = Heat Index (feels like)
// T = Temperature in Celsius
// R = Relative Humidity
float getFeelsLikeTemperature(float temp, float humidity) {
    float c1 = -8.78469475556;
    float c2 = 1.61139411;
    float c3 = 2.33854883889;
    float c4 = -0.14611605;
    float c5 = -0.012308094;
    float c6 = -0.0164248277778;
    float c7 = 0.002211732;
    float c8 = 0.00072546;
    float c9 = -0.000003582;

    float T = temp;
    float R = humidity;
    return c1 + c2 * T + c3 * R + c4 * T * R + c5 * T * T + c6 * R * R + c7 * T * T * R + c8 * T * R * R + c9 * T * T * R * R;
}