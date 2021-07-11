/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct _MZNInstance;
typedef struct _MZNInstance* MZNInstance;

void set_rnd_seed(int seed);

MZNInstance minizinc_instance_init(const char* mza_file, const char* data_file, const char* solver);
void minizinc_instance_destroy(MZNInstance);

void minizinc_add_call(MZNInstance, const char* call, ...);
void minizinc_set_solution(MZNInstance, int def, int sol);
void minizinc_output_dict(MZNInstance, bool);
void minizinc_print_hedge(MZNInstance);
void minizinc_set_limit(MZNInstance, int limit);

void minizinc_push_state(MZNInstance);
void minizinc_pop_state(MZNInstance);

const char* minizinc_solve(MZNInstance);

#ifdef __cplusplus
}
#endif
