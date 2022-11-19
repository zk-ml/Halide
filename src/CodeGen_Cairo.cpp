#include "PrintLoopNest.h"
#include "AllocationBoundsInference.h"
#include "Bounds.h"
#include "BoundsInference.h"
#include "FindCalls.h"
#include "Func.h"
#include "Function.h"
#include "IRPrinter.h"
#include "RealizationOrder.h"
#include "RemoveExternLoops.h"
#include "RemoveUndef.h"
#include "ScheduleFunctions.h"
#include "Simplify.h"
#include "SimplifyCorrelatedDifferences.h"
#include "SimplifySpecializations.h"
#include "SlidingWindow.h"
#include "Target.h"
#include "UniquifyVariableNames.h"
#include "WrapCalls.h"

#include "Lower.h"
#include "AddAtomicMutex.h"
#include "AddImageChecks.h"
#include "AddParameterChecks.h"
#include "AllocationBoundsInference.h"
#include "AsyncProducers.h"
#include "BoundSmallAllocations.h"
#include "Bounds.h"
#include "BoundsInference.h"
#include "CSE.h"
#include "CanonicalizeGPUVars.h"
#include "ClampUnsafeAccesses.h"
#include "CompilerLogger.h"
#include "Debug.h"
#include "DebugArguments.h"
#include "DebugToFile.h"
#include "Deinterleave.h"
#include "EarlyFree.h"
#include "ExtractTileOperations.h"
#include "FindCalls.h"
#include "FindIntrinsics.h"
#include "FlattenNestedRamps.h"
#include "Func.h"
#include "Function.h"
#include "FuseGPUThreadLoops.h"
#include "FuzzFloatStores.h"
#include "HexagonOffload.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "IRPrinter.h"
#include "InferArguments.h"
#include "InjectHostDevBufferCopies.h"
#include "Inline.h"
#include "LICM.h"
#include "LoopCarry.h"
#include "LowerParallelTasks.h"
#include "LowerWarpShuffles.h"
#include "Memoization.h"
#include "OffloadGPULoops.h"
#include "PartitionLoops.h"
#include "Prefetch.h"
#include "Profiling.h"
#include "PurifyIndexMath.h"
#include "Qualify.h"
#include "RealizationOrder.h"
#include "RebaseLoopsToZero.h"
#include "RemoveDeadAllocations.h"
#include "RemoveExternLoops.h"
#include "RemoveUndef.h"
#include "ScheduleFunctions.h"
#include "SelectGPUAPI.h"
#include "Simplify.h"
#include "SimplifyCorrelatedDifferences.h"
#include "SimplifySpecializations.h"
#include "SkipStages.h"
#include "SlidingWindow.h"
#include "SplitTuples.h"
#include "StorageFlattening.h"
#include "StorageFolding.h"
#include "StrictifyFloat.h"
#include "Substitute.h"
#include "Tracing.h"
#include "TrimNoOps.h"
#include "UnifyDuplicateLets.h"
#include "UniquifyVariableNames.h"
#include "UnpackBuffers.h"
#include "UnrollLoops.h"
#include "UnsafePromises.h"
#include "VectorizeLoops.h"
#include "WrapCalls.h"

#include <tuple>

namespace Halide {
namespace Internal {

using std::map;
using std::string;
using std::vector;

namespace {

class CodeGen_Cairo : public IRVisitor {
public:
    CodeGen_Cairo(std::ostream &output, const map<string, Function> &e)
        : out(output), env(e), indent(0) {
    }

private:
    std::ostream &out;
    const map<string, Function> &env;
    int indent;

    Scope<Expr> constants;

    using IRVisitor::visit;

    Indentation get_indent() const {
        return Indentation{indent};
    }

    string simplify_var_name(const string &s) {
        return simplify_name(s, false);
    }

    string simplify_func_name(const string &s) {
        return simplify_name(s, true);
    }

    string simplify_name(const string &s, bool is_func) {
        // Trim the function name and stage number from the for loop,
        // as well as any uniqueness $n suffixes on variables.
        std::ostringstream trimmed_name;

        bool keep = is_func;
        int dot_count = 0;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '.') {
                dot_count++;
                if (dot_count >= 2) {
                    if (dot_count == 2) {
                        i++;
                    }
                    keep = true;
                }
            }
            if (s[i] == '$') {
                keep = false;
            }
            if (keep) {
                trimmed_name << s[i];
            }
        }

        return trimmed_name.str();
    }

    void visit(const For *op) override {
        string simplified_loop_var_name = simplify_var_name(op->name);

        out << get_indent() << op->for_type << " " << simplified_loop_var_name;

        // If the min or extent are constants, print them. At this
        // stage they're all variables.
        Expr min_val = op->min, extent_val = op->extent;
        const Variable *min_var = min_val.as<Variable>();
        const Variable *extent_var = extent_val.as<Variable>();
        if (min_var && constants.contains(min_var->name)) {
            min_val = constants.get(min_var->name);
        }

        if (extent_var && constants.contains(extent_var->name)) {
            extent_val = constants.get(extent_var->name);
        }

        if (extent_val.defined() && is_const(extent_val) &&
            min_val.defined() && is_const(min_val)) {
            Expr max_val = simplify(min_val + extent_val - 1);
            out << " in [" << min_val << ", " << max_val << "]";
        }

        out << op->device_api;

        out << ":\n";
        indent += 2;
        op->body.accept(this);
        indent -= 2;
    }

    void visit(const Realize *op) override {
        // If the storage and compute levels for this function are
        // distinct, print the store level too.
        auto it = env.find(op->name);
        if (it != env.end() &&
            !(it->second.schedule().store_level() ==
              it->second.schedule().compute_level())) {
            out << get_indent();
            out << "store " << simplify_func_name(op->name) << ":\n";
            indent += 2;
            op->body.accept(this);
            indent -= 2;
        } else {
            op->body.accept(this);
        }
    }

    void visit(const ProducerConsumer *op) override {
        out << get_indent();
        if (op->is_producer) {
            out << "produce " << simplify_func_name(op->name) << ":\n";
        } else {
            out << "consume " << simplify_func_name(op->name) << ":\n";
        }
        indent += 2;
        op->body.accept(this);
        indent -= 2;
    }

    void visit(const Provide *op) override {
        out << get_indent() << simplify_func_name(op->name) << "(...) = ...\n";
    }

    void visit(const LetStmt *op) override {
        if (is_const(op->value)) {
            constants.push(op->name, op->value);
            op->body.accept(this);
            constants.pop(op->name);
        } else {
            op->body.accept(this);
        }
    }
};

}  // namespace

string print_cairo(const Module& mod) {

    // Now convert that to pseudocode
    std::ostringstream sstr;
    IRPrinter irp = IRPrinter(sstr);
    
    for (const auto &f : mod.functions()) {
        irp.print(f.body);
    }

    return sstr.str();
}

}  // namespace Internal
}  // namespace Halide
