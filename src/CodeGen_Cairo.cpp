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

#include "CodeGen_Cairo.h"

#include <tuple>

namespace Halide {
namespace Internal {

using std::map;
using std::ostream;
using std::ostringstream;
using std::string;
using std::vector;

CodeGen_Cairo::CodeGen_Cairo(ostream &s)
    : IRPrinter(s) {
    s << "Cairo Instructions:\n";
}

string print_cairo(const Module& mod) {

    // Now convert that to pseudocode
    std::ostringstream sstr;
    // IRPrinter irp = IRPrinter(sstr);
    
    // for (const auto &f : mod.functions()) {
    //   irp.print(f.body);
    // }

    CodeGen_Cairo pln(sstr);
    for (const auto &f : mod.functions()) {
        pln.print(f.body);
    }

    return sstr.str();
}

}  // namespace Internal
}  // namespace Halide
