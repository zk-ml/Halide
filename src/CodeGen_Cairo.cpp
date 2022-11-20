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

void CodeGen_Cairo::visit(const AssertStmt *op) {
    // stream << get_indent() << "assert\n";
    /*
    stream << get_indent() << "assert(";
    print_no_parens(op->condition);
    stream << ", ";
    print_no_parens(op->message);
    stream << ")\n";
    */
}

void CodeGen_Cairo::visit(const Call *op) {
    // TODO: Print indication of C vs C++?
    if (op->name == "add_image_checks_marker") {
        return;
    }
    if (op->name == "_halide_buffer_get_host") {
        print_list(op->args);
        return;
    }
    if (!known_type.contains(op->name) &&
        (op->type != Int(32))) {
        if (op->type.is_handle()) {
            stream << op->type;  // Already has parens
        } else {
            stream << "(" << op->type << ")";
        }
    }
    stream << op->name << "(";
    print_list(op->args);
    stream << ")";
}

void CodeGen_Cairo::visit(const Cast *op) {
    if (op->type == type_of<halide_buffer_t *>()) {
        print(op->value);
        return;
    }
    stream << op->type << "(";
    print(op->value);
    stream << ")";
}

void CodeGen_Cairo::visit(const Variable *op) {
    if (op->type == type_of<halide_buffer_t *>()) {
        stream << op->name;
        return;
    }
    if (!known_type.contains(op->name) &&
        (op->type != Int(32))) {
        // Handle types already have parens
        if (op->type.is_handle()) {
            stream << op->type;
        } else {
            stream << "(" << op->type << ")";
        }
    }
    stream << op->name;
}

void CodeGen_Cairo::visit(const Let *op) {
    ScopedBinding<> bind(known_type, op->name);
    open();
    stream << op->name << " = ";
    print(op->value);
    stream << " in ";
    print(op->body);
    close();
}

void CodeGen_Cairo::visit(const LetStmt *op) {
    ScopedBinding<> bind(known_type, op->name);
    stream << get_indent() << op->name << " = ";
    print_no_parens(op->value);
    stream << "\n";

    print(op->body);
}

void CodeGen_Cairo::visit(const ProducerConsumer *op) {
    stream << get_indent();
    if (op->is_producer) {
        // stream << "produce " << op->name << " {\n";
    } else {
        assert(false);
        // stream << "consume " << op->name << " {\n";
    }
    // indent++;
    print(op->body);
    // indent--;
    // stream << get_indent() << "}\n";
}

string print_cairo(const Module& mod) {

    // Now convert that to pseudocode
    std::ostringstream sstr;
    // IRPrinter irp = IRPrinter(sstr);
    
    // for (const auto &f : mod.functions()) {
    //   irp.print(f.body);
    // }

    sstr << "Input Buffers: \n";
    for (const auto &b : mod.buffers()) {
        sstr << b.name() << "\n";
    }
    sstr << "\n";

    for (const auto &f : mod.functions()) {
        sstr << f.name << " arg Buffers: \n" ;
        for (const auto &a : f.args) {
            sstr << a.name << "\n";
        }
    }
    sstr << "\n";

    CodeGen_Cairo pln(sstr);
    for (const auto &f : mod.functions()) {
        sstr << f.name << "\n";
        pln.print(f.body);
    }

    return sstr.str();
}

}  // namespace Internal
}  // namespace Halide
