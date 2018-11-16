#include <list>
#include <string>
#include <iostream>
#include <fstream>
#include "rewriter.hpp"
#include "json/json.h"

void parseConditions(const Json::Value& conditions,
                     std::list<Condition>& r_conditions)
{
    for (const auto& condition : conditions) {
        Condition r_condition;
        r_condition.name = condition["query"][0].asString();

        for (uint i = 1; i < condition["query"].size(); i++) {
            r_condition.arguments.push_back(condition["query"][i].asString());
        }

        for (uint i = 0; i < condition["expectedResults"].size(); i++) {
            r_condition.expectedValues.push_back(
                        condition["expectedResults"][i].asString());
        }

        r_conditions.push_back(r_condition);
    }
}

BinOpType getType(const std::string& type) {
    if (type == "i16")
        return BinOpType::INT16;
    if (type == "i32")
        return BinOpType::INT32;
    if (type == "i64")
        return BinOpType::INT64;
    if (type == "i8")
        return BinOpType::INT8;

    return BinOpType::NBOP;
}

void parseRule(const Json::Value& rule, RewriteRule& r) {
    // Get findInstructions
    for (const auto& findInstruction : rule["findInstructions"]) {
        InstrumentInstruction instr;
        instr.returnValue = findInstruction["returnValue"].asString();
        if (instr.returnValue == "") {
            instr.returnValue = "*";
        }

        instr.instruction = findInstruction["instruction"].asString();
        for (const auto& operand : findInstruction["operands"]) {
            instr.parameters.push_back(operand.asString());
        }

        instr.getSizeTo = findInstruction["getTypeSize"].asString();
        instr.type = getType(findInstruction["type"].asString());
        instr.getDestType = findInstruction["getDestType"].asString();

        for (const auto& info : findInstruction["getPointerInfo"]) {
            instr.getPointerInfoTo.push_back(info.asString());
        }

        for (const auto& info : findInstruction["getPointerInfoMin"]) {
            instr.getPointerInfoMinTo.push_back(info.asString());
        }

        for (const auto& info : findInstruction["getPointerInfoMinMax"]) {
            instr.getPInfoMinMaxTo.push_back(info.asString());
        }

        instr.stripInboundsOffsets =
                        findInstruction["stripInboundsOffsets"].asString();

        r.foundInstrs.push_back(instr);
    }

    // Get newInstruction
    r.newInstr.returnValue = rule["newInstruction"]["returnValue"].asString();
    r.newInstr.instruction = rule["newInstruction"]["instruction"].asString();
    for (const auto& op : rule["newInstruction"]["operands"]) {
        r.newInstr.parameters.push_back(op.asString());
    }

    // Get placement, in and remember field
    if (rule["where"] == "before") {
        r.where = InstrumentPlacement::BEFORE;
    }
    else if (rule["where"] == "after") {
        r.where = InstrumentPlacement::AFTER;
    }
    else if (rule["where"] == "replace") {
        r.where = InstrumentPlacement::REPLACE;
    }
    else if (rule["where"] == "return") {
        r.where = InstrumentPlacement::RETURN;
    }
    else if (rule["where"] == "entry") {
        r.where = InstrumentPlacement::ENTRY;
    }

    r.inFunction = rule["in"].asString();
    r.remember = rule["remember"].asString();
    r.rememberPTSet = rule["rememberPTSet"].asString();

    // Get conditions
    parseConditions(rule["conditions"], r.conditions);
    if (rule["mustHoldForAll"].asString() == "true")
        r.mustHoldForAll = true;

    for (const auto& setFlag : rule["setFlags"]) {
        r.setFlags.insert(Flag(setFlag[0].asString(), setFlag[1].asString()));
    }
}

void parseGlobalRule(const Json::Value& global_rule,
                     GlobalVarsRule& r_global_rule)
{
    // Get rule for global variables
    r_global_rule.globalVar.globalVariable =
                    global_rule["findGlobals"]["globalVariable"].asString();
    r_global_rule.globalVar.getSizeTo =
                    global_rule["findGlobals"]["getTypeSize"].asString();

    // Get conditions
    parseConditions(global_rule["conditions"], r_global_rule.conditions);
    if (global_rule["mustHoldForAll"].asString() == "true")
        r_global_rule.mustHoldForAll = true;

    r_global_rule.newInstr.returnValue =
                    global_rule["newInstruction"]["returnValue"].asString();
    r_global_rule.newInstr.instruction =
                    global_rule["newInstruction"]["instruction"].asString();

    for (auto operand : global_rule["newInstruction"]["operands"]) {
        r_global_rule.newInstr.parameters.push_back(operand.asString());
    }

    r_global_rule.inFunction = global_rule["in"].asString();
}

void parsePhase(const Json::Value& phase, Phase& r_phase) {
    // Load instructions rules for instructions
    for (const auto& rule : phase["instructionsRules"]) {
        RewriteRule rw_rule;
        parseRule(rule, rw_rule);
        r_phase.config.push_back(rw_rule);
    }

    // Load global variables rules for instructions
    for (const auto& rule : phase["globalVariablesRules"]) {
        GlobalVarsRule g_rule;
        parseGlobalRule(rule, g_rule);
        r_phase.gconfig.push_back(g_rule);
    }
}

void Rewriter::parseConfig(std::ifstream &config_file) {
    Json::Value json_rules;
    bool parsingSuccessful;

#if (JSONCPP_VERSION_MINOR < 8 || (JSONCPP_VERSION_MINOR == 8 && JSONCPP_VERSION_PATCH < 1))
    Json::Reader reader;
    parsingSuccessful = reader.parse(config_file, json_rules);
    if (!parsingSuccessful) {
        cerr  << "Failed to parse configuration\n"
              << reader.getFormattedErrorMessages();
        throw runtime_error("Config parsing failure.");
    }

#else
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;
    parsingSuccessful = Json::parseFromStream(rbuilder, config_file,
                                              &json_rules, &errs);
    if (!parsingSuccessful) {
        std::cerr  << "Failed to parse configuration\n"
              << errs;
        throw std::runtime_error("Config parsing failure.");
    }

#endif

    // Load paths to analyses
    for (const auto& analysis : json_rules["analyses"]) {
        this->analysisPaths.push_back(analysis.asString());
    }

    // Load flags
    for (const auto& flag : json_rules["flags"]) {
        this->flags.insert(Flag(flag.asString(), ""));
    }

    // Load phases
    for (const auto& phase : json_rules["phases"]) {
        Phase rw_phase;
        parsePhase(phase, rw_phase);
        this->phases.push_back(rw_phase);
    }
}

const Phases& Rewriter::getPhases() {
    return this->phases;
}

bool Rewriter::isFlag(std::string name) {
    auto search = this->flags.find(name);
    return search != this->flags.end();
}

void Rewriter::setFlag(std::string name, std::string value) {
    auto search = this->flags.find(name);
    if (search != this->flags.end())
            search->second = value;
}

std::string Rewriter::getFlagValue(std::string name) {
    auto search = this->flags.find(name);
    if (search != this->flags.end())
        return search->second;

    return "";
}

