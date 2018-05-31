#include "range_analysis_plugin.hpp"
#include <limits>
#include <cmath>

using namespace llvm;

bool checkUnknown(const Range& a, const Range& b) {
    if (!a.isRegular() || !b.isRegular())
        return true;

    if (a.isMaxRange() || b.isMaxRange())
        return true;
}


std::string RangeAnalysisPlugin::canOverflow(Instruction* inst) {
    const auto* intT = dyn_cast<IntegerType>(inst->getType());
    if (!intT)
        return "unknown";

    auto result = RA.find(inst->getFunction());
    if (result == RA.end())
        return "unknown";

    ConstraintGraph& CG = result->second;

    if (const auto* binOp
        = dyn_cast<OverflowingBinaryOperator>(inst)) {
        // we are looking for bin. ops with nsw
        if (!binOp->hasNoSignedWrap())
            return "false";

        Range a = getRange(CG, binOp->getOperand(0));
        Range b = getRange(CG, binOp->getOperand(1));

        // check for unknown ranges
        if (checkUnknown(a, b))
            return "true";

        // check addition
        if (const auto* binOp
        = dyn_cast<AddOperator>(inst)) {
            return canOverflowAdd(a, b, *intT);
        }

        // check subtraction
        if (const auto* binOp
        = dyn_cast<SubOperator>(inst)) {
            return canOverflowSub(a, b, *intT);
        }

        // check multiplication
        if (const auto* binOp
        = dyn_cast<MulOperator>(inst)) {
            return canOverflowMul(a, b, *intT);
        }
    }

    // check division
    if (const auto* binOp
        = dyn_cast<SDivOperator>(inst)) {
        Range a = getRange(CG, binOp->getOperand(0));
        Range b = getRange(CG, binOp->getOperand(1));
        if (checkUnknown(a, b))
            return "true";
        return canOverflowDiv(a, b, *intT);
    }

    return "unknown";
}

Range RangeAnalysisPlugin::getRange(ConstraintGraph& CG,
                        llvm::Value* val)
{
    Range r = CG.getRange(val);
    if (r.isMaxRange() || r.isUnknown()) {
        if (auto* loadInst = llvm::dyn_cast<llvm::LoadInst>(val)) {
            return CG.getRange(loadInst->getOperand(0));
        }
    }

    return r;
}

bool checkOverflowAdd(APInt ax, APInt ay, const IntegerType& t) {
    double x = ax.signedRoundToDouble();
    double y = ay.signedRoundToDouble();

    if((x > 0) && (y > 0) && (x > (std::pow(2, t.getBitWidth() - 1) - 1) - y)) {
        return true;
    }

    if((x < 0) && (y < 0) && (x < (-std::pow(2, t.getBitWidth() - 1)) - y)) {
        return true;
    }
}

std::string RangeAnalysisPlugin::canOverflowAdd(const Range& a,
        const Range& b, const IntegerType& t)
{
    if (checkOverflowAdd(a.getUpper(), b.getUpper(), t)) {
        return "true";
    }

    if (checkOverflowAdd(a.getLower(), b.getLower(), t)) {
        return "true";
    }
}

bool checkOverflowSub(APInt ax, APInt ay, const IntegerType& t) {
    double x = ax.signedRoundToDouble();
    double y = ay.signedRoundToDouble();

    if((y > 0) && (x < (-std::pow(2, t.getBitWidth() - 1)) +  y)) {
        return true;
    }

    if((y < 0) && (x > (std::pow(2, t.getBitWidth() - 1) - 1) + y)) {
        return true;
    }
}

std::string RangeAnalysisPlugin::canOverflowSub(const Range& a,
        const Range& b, const IntegerType& t)
{
    if (checkOverflowSub(a.getUpper(), b.getUpper(), t)) {
        return "true";
    }

    if (checkOverflowSub(a.getLower(), b.getLower(), t)) {
        return "true";
    }

    if (checkOverflowSub(a.getUpper(), b.getLower(), t)) {
        return "true";
    }

    if (checkOverflowSub(a.getLower(), b.getUpper(), t)) {
        return "true";
    }
}

bool checkOverflowMul(APInt ax, APInt ay, const IntegerType& t) {
    double x = ax.signedRoundToDouble();
    double y = ay.signedRoundToDouble();

    if (x > (std::pow(2, t.getBitWidth() - 1) - 1) / y) {
        return true;
    }
    if ((x < (-std::pow(2, t.getBitWidth() - 1)) / y)) {
        return true;
    }
}

std::string RangeAnalysisPlugin::canOverflowMul(const Range& a,
        const Range& b, const IntegerType& t)
{
    if (checkOverflowMul(a.getUpper(), b.getUpper(), t)) {
        return "true";
    }

    if (checkOverflowMul(a.getLower(), b.getLower(), t)) {
        return "true";
    }

    if (checkOverflowMul(a.getUpper(), b.getLower(), t)) {
        return "true";
    }

    if (checkOverflowMul(a.getLower(), b.getUpper(), t)) {
        return "true";
    }

    if (a.getLower().signedRoundToDouble() <= (-std::pow(2, t.getBitWidth()))
         && b.getLower().signedRoundToDouble() <= -1
         && b.getUpper().signedRoundToDouble() >= -1)
        return "true";

    if (b.getLower().signedRoundToDouble() <= (-std::pow(2, t.getBitWidth()))
         && a.getLower().signedRoundToDouble() <= -1
         && a.getUpper().signedRoundToDouble() >= -1)
        return "true";

}

std::string RangeAnalysisPlugin::canOverflowDiv(const Range& a,
        const Range& b, const IntegerType& t)
{
    // TODO: Check also for division by zero?
    // if (b.getLower() <= 0 && b.getUpper() >= 0)
    //    return "true";

    if (a.getLower().signedRoundToDouble() <= (-std::pow(2, t.getBitWidth() - 1))
         && b.getLower().signedRoundToDouble() <= -1
         && b.getUpper().signedRoundToDouble() >= -1)
        return "true";
}

extern "C" InstrPlugin* create_object(llvm::Module* module) {
    return new RangeAnalysisPlugin(module);
}
