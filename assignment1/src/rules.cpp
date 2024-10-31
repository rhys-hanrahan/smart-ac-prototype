#include "rules.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ctime>

std::vector<RuleSet> rules;
ACState ac_state = {false, 22.0, "cool", false}; // Default state

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

// Helper to get the current season based on month in AU
String getCurrentSeason() {
    time_t now = time(nullptr);
    struct tm *timeInfo = localtime(&now);
    int month = timeInfo->tm_mon + 1; // tm_mon is months since January [0, 11]

    if (month >= 3 && month <= 5) return "autumn";  // March to May
    if (month >= 6 && month <= 8) return "winter";  // June to August
    if (month >= 9 && month <= 11) return "spring"; // September to November
    return "summer"; // December to February
}

// Helper function to load rules from SPIFFS
void loadRules() {
    File file = SPIFFS.open("/rules.json", "r");
    if (!file) {
        Serial.println("Failed to open rules file");
        return;
    }

    DynamicJsonDocument doc(4096); // Adjust as needed
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

        JsonObject timeframe = ruleObj["timeframe"];
        for (JsonVariant day : timeframe["days"].as<JsonArray>()) {
            rule.timeframe.days.push_back(day.as<String>());
        }
        rule.timeframe.start_time = timeframe["start_time"].as<String>();
        rule.timeframe.end_time = timeframe["end_time"].as<String>();
        for (JsonVariant season : timeframe["seasons"].as<JsonArray>()) {
            rule.timeframe.seasons.push_back(season.as<String>());
        }

        for (JsonObject conditionObj : ruleObj["conditions"].as<JsonArray>()) {
            Condition condition;
            condition.field = conditionObj["field"].as<String>();
            condition.operator_ = conditionObj["operator"].as<String>();
            condition.value = conditionObj["value"].as<float>();
            rule.conditions.push_back(condition);
        }

        for (JsonObject actionObj : ruleObj["actions"].as<JsonArray>()) {
            Action action;
            action.type = actionObj["type"].as<String>();
            action.target_temp = actionObj["target_temp"].as<float>();
            action.increment_value = actionObj["increment_value"].as<float>();
            rule.actions.push_back(action);
        }
        Serial.printf("Rule loaded: %s\n", rule.name.c_str());
        Serial.printf("Conditions: %d, Actions: %d\n", rule.conditions.size(), rule.actions.size());
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

    DynamicJsonDocument doc(4096); // Adjust as needed
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

        JsonArray conditions = ruleObj.createNestedArray("conditions");
        for (const Condition &condition : rule.conditions) {
            JsonObject conditionObj = conditions.createNestedObject();
            conditionObj["field"] = condition.field;
            conditionObj["operator"] = condition.operator_;
            conditionObj["value"] = condition.value;
        }

        JsonArray actions = ruleObj.createNestedArray("actions");
        for (const Action &action : rule.actions) {
            JsonObject actionObj = actions.createNestedObject();
            actionObj["type"] = action.type;
            actionObj["target_temp"] = action.target_temp;
            actionObj["increment_value"] = action.increment_value;
        }
    }

    serializeJson(doc, file);
    file.close();
    Serial.println("Rules saved successfully");
    Serial.printf("Saved %d rules\n", rules.size());
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
bool areConditionsMet(const std::vector<Condition> &conditions) {
    for (const Condition &condition : conditions) {
        float value = 0.0;

        if (condition.field == "temperature") value = ac_state.current_temp;
        else if (condition.field == "feels_like") value = ac_state.current_temp + 1.0; // Placeholder for actual feels-like value

        if (condition.operator_ == ">" && !(value > condition.value)) return false;
        if (condition.operator_ == "<" && !(value < condition.value)) return false;
        if (condition.operator_ == ">=" && !(value >= condition.value)) return false;
        if (condition.operator_ == "<=" && !(value <= condition.value)) return false;
        if (condition.operator_ == "within" && !(value >= condition.start.toFloat() && value <= condition.end.toFloat())) return false;
    }
    return true;
}

// Function to execute actions based on the rule's configuration
void executeAction(const Action &action) {
    if (action.type == "set_temp") {
        ac_state.current_temp = action.target_temp;
        Serial.printf("AC temperature set to %.2f\n", ac_state.current_temp);
    } else if (action.type == "increment_temp") {
        ac_state.current_temp += action.increment_value;
        Serial.printf("AC temperature incremented to %.2f\n", ac_state.current_temp);
    }
}

// Main function to evaluate all rules and execute actions if conditions are met
void evaluateRules() {
    for (const RuleSet &rule : rules) {
        if (isTimeframeValid(rule.timeframe) && areConditionsMet(rule.conditions)) {
            Serial.printf("Executing actions for rule: %s\n", rule.name.c_str());
            for (const Action &action : rule.actions) {
                executeAction(action);
            }
        }
    }
}