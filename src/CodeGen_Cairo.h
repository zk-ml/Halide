#ifndef HALIDE_CODEGEN_CAIRO_H
#define HALIDE_CODEGEN_CAIRO_H

/** \file
 *
 * Defines methods to print out the loop nest corresponding to a schedule.
 */

#include "IRPrinter.h"
#include <string>
#include <vector>

namespace Halide {
namespace Internal {

class CodeGen_Cairo: public IRPrinter {
public:
    CodeGen_Cairo(std::ostream &dest);
};

class Function;

/** Emit some simple pseudocode that shows the structure of the loop
 * nest specified by this pipeline's schedule, and the schedules of
 * the functions it uses. */
std::string print_cairo(const Module &output_funcs);

}  // namespace Internal
}  // namespace Halide

#endif
