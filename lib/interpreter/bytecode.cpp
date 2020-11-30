/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 *     Guido Tack <guido.tack@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <minizinc/interpreter.hh>
#include <minizinc/interpreter/bytecode.hh>

namespace MiniZinc {

const std::string BytecodeProc::mode_to_string[] = {"ROOT",    "ROOT_NEG", "FUN",
                                                    "FUN_NEG", "IMP",      "IMP_NEG"};

std::string BytecodeStream::toString(const std::vector<BytecodeProc>& procs) const {
  std::ostringstream oss;
  int pc = 0;
  while (pc < _bs.size()) {
    int cur_pc = pc;
    switch (instr(pc)) {
      case BytecodeStream::ADDI: {
        oss << "ADDI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::SUBI: {
        oss << "SUBI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::MULI: {
        oss << "MULI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::DIVI: {
        oss << "DIVI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::MODI: {
        oss << "MODI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::INCI: {
        oss << "INCI R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::DECI: {
        oss << "DECI R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::IMMI: {
        oss << "IMMI " << intval(pc).toIntVal() << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::CLEAR: {
        oss << "CLEAR "
            << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::LOAD_GLOBAL: {
        oss << "LOAD_GLOBAL " << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::STORE_GLOBAL: {
        oss << "STORE_GLOBAL R" << reg(pc) << " " << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::MOV: {
        oss << "MOV R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::JMP: {
        oss << "JMP " << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::JMPIF: {
        oss << "JMPIF R" << reg(pc) << " " << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::JMPIFNOT: {
        oss << "JMPIFNOT R" << reg(pc) << " " << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::EQI: {
        oss << "EQI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::LTI: {
        oss << "LTI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::LEI: {
        oss << "LEI R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::AND: {
        oss << "AND R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::OR: {
        oss << "OR R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::NOT: {
        oss << "NOT R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::XOR: {
        oss << "XOR R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::ISPAR: {
        oss << "ISPAR R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::ISEMPTY: {
        oss << "ISEMPTY R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::LENGTH: {
        oss << "LENGTH R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::GET_VEC: {
        oss << "GET_VEC R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc
            << "\n";
      } break;
      case BytecodeStream::GET_ARRAY: {
        oss << "GET_ARRAY ";
        long long int n = intval(pc).toInt();
        oss << n;
        for (long long int i = 0; i < (n + 3); ++i) {
          oss << " R" << reg(pc);
        }
        oss << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::LB: {
        oss << "LB R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::UB: {
        oss << "UB R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::DOM: {
        oss << "DOM R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::MAKE_SET: {
        oss << "MAKE_SET R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::INTERSECTION: {
        oss << "INTERSECTION R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc
            << "\n";
      } break;
      case BytecodeStream::UNION: {
        oss << "UNION R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc
            << "\n";
      } break;
      case BytecodeStream::DIFF: {
        oss << "DIFF R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::INTERSECT_DOMAIN: {
        oss << "INTERSECT_DOMAIN R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % "
            << cur_pc << "\n";
      } break;
      case BytecodeStream::RET: {
        oss << "RET"
            << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::CALL: {
        auto m = static_cast<BytecodeProc::Mode>(chr(pc));
        int p = reg(pc);
        bool cse = chr(pc);
        assert(!procs.empty());
        oss << "CALL " << BytecodeProc::mode_to_string[m] << " " << procs[p].name
            << (cse ? "" : " no_cse") << " ";
        oss << procs[p].nargs;
        for (int i = 0; i < procs[p].nargs; i++) {
          oss << " R" << reg(pc);
        }
        oss << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::BUILTIN: {
        int p = reg(pc);
        oss << "BUILTIN " << p << " ";
        oss << procs[p].nargs;
        for (int i = 0; i < procs[p].nargs; i++) {
          oss << " R" << reg(pc);
        }
        oss << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::TCALL: {
        auto m = static_cast<BytecodeProc::Mode>(chr(pc));
        int p = reg(pc);
        bool cse = chr(pc);

        if (procs.empty()) {
          oss << "TCALL " << BytecodeProc::mode_to_string[m] << " " << p << (cse ? "" : " no_cse")
              << " % " << cur_pc << "\n";
        } else {
          oss << "TCALL " << BytecodeProc::mode_to_string[m] << " " << procs[p].name
              << (cse ? "" : " no_cse") << " % " << cur_pc << "\n";
        }
      } break;
      case BytecodeStream::ITER_ARRAY: {
        oss << "ITER_ARRAY" << reg(pc) << " " << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::ITER_VEC: {
        oss << "ITER_VEC " << reg(pc) << " " << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::ITER_RANGE: {
        oss << "ITER_RANGE " << reg(pc) << " " << reg(pc) << " " << reg(pc) << " % " << cur_pc
            << "\n";
      } break;
      case BytecodeStream::ITER_NEXT: {
        oss << "ITER_NEXT R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::ITER_BREAK: {
        oss << "ITER_BREAK " << intval(pc).toString() << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::OPEN_AGGREGATION: {
        oss << "OPEN_AGGREGATION ";
        int p = chr(pc);
        switch (p) {
          case AggregationCtx::VCTX_AND:
            oss << "AND";
            break;
          case AggregationCtx::VCTX_OR:
            oss << "OR";
            break;
          case AggregationCtx::VCTX_VEC:
            oss << "VEC";
            break;
          case AggregationCtx::VCTX_OTHER:
            oss << "OTHER";
            break;
          default:
            oss << "ERROR";
            assert(false);
            break;
        }
        oss << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::CLOSE_AGGREGATION: {
        oss << "CLOSE_AGGREGATION"
            << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::SIMPLIFY_LIN: {
        oss << "SIMPLIFY_LIN R" << reg(pc) << " R" << reg(pc) << " " << intval(pc).toIntVal()
            << " R" << reg(pc) << " R" << reg(pc) << " R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::PUSH: {
        oss << "PUSH R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::POP: {
        oss << "POP R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::POST: {
        oss << "POST R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::TRACE: {
        oss << "TRACE R" << reg(pc) << " % " << cur_pc << "\n";
      } break;
      case BytecodeStream::ABORT: {
        oss << "ABORT"
            << " % " << cur_pc << "\n";
      } break;
    }
  }
  return oss.str();
}

}  // namespace MiniZinc
