/*
 * Copyright (c) 2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "rose_build_engine_blob.h"
#include "rose_build_program.h"
#include "util/container.h"
#include "util/multibit_build.h"
#include "util/verify_types.h"

#include <algorithm>
#include <cstring>

using namespace std;

namespace ue2 {

/* Destructors to avoid weak vtables. */

RoseInstruction::~RoseInstruction() = default;
RoseInstrCatchUp::~RoseInstrCatchUp() = default;
RoseInstrCatchUpMpv::~RoseInstrCatchUpMpv() = default;
RoseInstrSomZero::~RoseInstrSomZero() = default;
RoseInstrSuffixesEod::~RoseInstrSuffixesEod() = default;
RoseInstrMatcherEod::~RoseInstrMatcherEod() = default;
RoseInstrEnd::~RoseInstrEnd() = default;

using OffsetMap = RoseInstruction::OffsetMap;

static
u32 calc_jump(const OffsetMap &offset_map, const RoseInstruction *from,
              const RoseInstruction *to) {
    DEBUG_PRINTF("computing relative jump from %p to %p\n", from, to);
    assert(from && contains(offset_map, from));
    assert(to && contains(offset_map, to));

    u32 from_offset = offset_map.at(from);
    u32 to_offset = offset_map.at(to);
    DEBUG_PRINTF("offsets: %u -> %u\n", from_offset, to_offset);
    assert(from_offset <= to_offset);

    return to_offset - from_offset;
}

void RoseInstrAnchoredDelay::write(void *dest, RoseEngineBlob &blob,
                                   const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->groups = groups;
    inst->done_jump = calc_jump(offset_map, this, target);
}

void RoseInstrCheckLitEarly::write(void *dest, RoseEngineBlob &blob,
                                  const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->min_offset = min_offset;
}

void RoseInstrCheckGroups::write(void *dest, RoseEngineBlob &blob,
                                 const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->groups = groups;
}

void RoseInstrCheckOnlyEod::write(void *dest, RoseEngineBlob &blob,
                                  const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->fail_jump = calc_jump(offset_map, this, target);
}

void RoseInstrCheckBounds::write(void *dest, RoseEngineBlob &blob,
                                 const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->min_bound = min_bound;
    inst->max_bound = max_bound;
    inst->fail_jump = calc_jump(offset_map, this, target);
}

void RoseInstrCheckNotHandled::write(void *dest, RoseEngineBlob &blob,
                                     const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->key = key;
    inst->fail_jump = calc_jump(offset_map, this, target);
}

void RoseInstrCheckLookaround::write(void *dest, RoseEngineBlob &blob,
                                     const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->index = index;
    inst->count = count;
    inst->fail_jump = calc_jump(offset_map, this, target);
}

void RoseInstrCheckMask::write(void *dest, RoseEngineBlob &blob,
                               const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->and_mask = and_mask;
    inst->cmp_mask = cmp_mask;
    inst->neg_mask = neg_mask;
    inst->offset = offset;
    inst->fail_jump = calc_jump(offset_map, this, target);
}

void RoseInstrCheckMask32::write(void *dest, RoseEngineBlob &blob,
                                 const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    copy(begin(and_mask), end(and_mask), inst->and_mask);
    copy(begin(cmp_mask), end(cmp_mask), inst->cmp_mask);
    inst->neg_mask = neg_mask;
    inst->offset = offset;
    inst->fail_jump = calc_jump(offset_map, this, target);
}

void RoseInstrCheckByte::write(void *dest, RoseEngineBlob &blob,
                               const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->and_mask = and_mask;
    inst->cmp_mask = cmp_mask;
    inst->negation = negation;
    inst->offset = offset;
    inst->fail_jump = calc_jump(offset_map, this, target);
}

void RoseInstrCheckInfix::write(void *dest, RoseEngineBlob &blob,
                                const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->queue = queue;
    inst->lag = lag;
    inst->report = report;
    inst->fail_jump = calc_jump(offset_map, this, target);
}

void RoseInstrCheckPrefix::write(void *dest, RoseEngineBlob &blob,
                                 const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->queue = queue;
    inst->lag = lag;
    inst->report = report;
    inst->fail_jump = calc_jump(offset_map, this, target);
}

void RoseInstrPushDelayed::write(void *dest, RoseEngineBlob &blob,
                                 const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->delay = delay;
    inst->index = index;
}

void RoseInstrRecordAnchored::write(void *dest, RoseEngineBlob &blob,
                                    const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->id = id;
}

void RoseInstrSomAdjust::write(void *dest, RoseEngineBlob &blob,
                               const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->distance = distance;
}

void RoseInstrSomLeftfix::write(void *dest, RoseEngineBlob &blob,
                                const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->queue = queue;
    inst->lag = lag;
}

void RoseInstrSomFromReport::write(void *dest, RoseEngineBlob &blob,
                                   const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->som = som;
}

void RoseInstrTriggerInfix::write(void *dest, RoseEngineBlob &blob,
                                  const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->cancel = cancel;
    inst->queue = queue;
    inst->event = event;
}

void RoseInstrTriggerSuffix::write(void *dest, RoseEngineBlob &blob,
                                   const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->queue = queue;
    inst->event = event;
}

void RoseInstrDedupe::write(void *dest, RoseEngineBlob &blob,
                            const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->quash_som = quash_som;
    inst->dkey = dkey;
    inst->offset_adjust = offset_adjust;
    inst->fail_jump = calc_jump(offset_map, this, target);
}

void RoseInstrDedupeSom::write(void *dest, RoseEngineBlob &blob,
                               const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->quash_som = quash_som;
    inst->dkey = dkey;
    inst->offset_adjust = offset_adjust;
    inst->fail_jump = calc_jump(offset_map, this, target);
}

void RoseInstrReportChain::write(void *dest, RoseEngineBlob &blob,
                                 const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->event = event;
    inst->top_squash_distance = top_squash_distance;
}

void RoseInstrReportSomInt::write(void *dest, RoseEngineBlob &blob,
                                 const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->som = som;
}

void RoseInstrReportSomAware::write(void *dest, RoseEngineBlob &blob,
                                    const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->som = som;
}

void RoseInstrReport::write(void *dest, RoseEngineBlob &blob,
                            const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->onmatch = onmatch;
    inst->offset_adjust = offset_adjust;
}

void RoseInstrReportExhaust::write(void *dest, RoseEngineBlob &blob,
                                   const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->onmatch = onmatch;
    inst->offset_adjust = offset_adjust;
    inst->ekey = ekey;
}

void RoseInstrReportSom::write(void *dest, RoseEngineBlob &blob,
                               const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->onmatch = onmatch;
    inst->offset_adjust = offset_adjust;
}

void RoseInstrReportSomExhaust::write(void *dest, RoseEngineBlob &blob,
                                      const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->onmatch = onmatch;
    inst->offset_adjust = offset_adjust;
    inst->ekey = ekey;
}

void RoseInstrDedupeAndReport::write(void *dest, RoseEngineBlob &blob,
                                     const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->quash_som = quash_som;
    inst->dkey = dkey;
    inst->onmatch = onmatch;
    inst->offset_adjust = offset_adjust;
    inst->fail_jump = calc_jump(offset_map, this, target);
}

void RoseInstrFinalReport::write(void *dest, RoseEngineBlob &blob,
                                 const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->onmatch = onmatch;
    inst->offset_adjust = offset_adjust;
}

void RoseInstrCheckExhausted::write(void *dest, RoseEngineBlob &blob,
                                    const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->ekey = ekey;
    inst->fail_jump = calc_jump(offset_map, this, target);
}

void RoseInstrCheckMinLength::write(void *dest, RoseEngineBlob &blob,
                                    const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->end_adj = end_adj;
    inst->min_length = min_length;
    inst->fail_jump = calc_jump(offset_map, this, target);
}

void RoseInstrSetState::write(void *dest, RoseEngineBlob &blob,
                              const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->index = index;
}

void RoseInstrSetGroups::write(void *dest, RoseEngineBlob &blob,
                              const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->groups = groups;
}

void RoseInstrSquashGroups::write(void *dest, RoseEngineBlob &blob,
                                  const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->groups = groups;
}

void RoseInstrCheckState::write(void *dest, RoseEngineBlob &blob,
                                const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->index = index;
    inst->fail_jump = calc_jump(offset_map, this, target);
}

void RoseInstrSparseIterBegin::write(void *dest, RoseEngineBlob &blob,
                                     const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->fail_jump = calc_jump(offset_map, this, target);

    // Resolve and write the multibit sparse iterator and the jump table.
    vector<u32> keys;
    vector<u32> jump_offsets;
    for (const auto &jump : jump_table) {
        keys.push_back(jump.first);
        assert(contains(offset_map, jump.second));
        jump_offsets.push_back(offset_map.at(jump.second));
    }

    vector<mmbit_sparse_iter> iter;
    mmbBuildSparseIterator(iter, keys, num_keys);
    assert(!iter.empty());
    inst->iter_offset = blob.add(iter.begin(), iter.end());
    inst->jump_table = blob.add(jump_offsets.begin(), jump_offsets.end());

    // Store offsets for corresponding SPARSE_ITER_NEXT operations.
    is_written = true;
    iter_offset = inst->iter_offset;
    jump_table_offset = inst->jump_table;
}

void RoseInstrSparseIterNext::write(void *dest, RoseEngineBlob &blob,
                                    const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->state = state;
    inst->fail_jump = calc_jump(offset_map, this, target);

    // Use the same sparse iterator and jump table as the SPARSE_ITER_BEGIN
    // instruction.
    assert(begin);
    assert(contains(offset_map, begin));
    assert(begin->is_written);
    inst->iter_offset = begin->iter_offset;
    inst->jump_table = begin->jump_table_offset;
}

void RoseInstrSparseIterAny::write(void *dest, RoseEngineBlob &blob,
                                   const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->fail_jump = calc_jump(offset_map, this, target);

    // Write the multibit sparse iterator.
    vector<mmbit_sparse_iter> iter;
    mmbBuildSparseIterator(iter, keys, num_keys);
    assert(!iter.empty());
    inst->iter_offset = blob.add(iter.begin(), iter.end());
}

void RoseInstrEnginesEod::write(void *dest, RoseEngineBlob &blob,
                                const OffsetMap &offset_map) const {
    RoseInstrBase::write(dest, blob, offset_map);
    auto *inst = static_cast<impl_type *>(dest);
    inst->iter_offset = iter_offset;
}

static
OffsetMap makeOffsetMap(const RoseProgram &program, u32 *total_len) {
    OffsetMap offset_map;
    u32 offset = 0;
    for (const auto &ri : program) {
        offset = ROUNDUP_N(offset, ROSE_INSTR_MIN_ALIGN);
        DEBUG_PRINTF("instr %p (opcode %d) -> offset %u\n", ri.get(),
                     ri->code(), offset);
        assert(!contains(offset_map, ri.get()));
        offset_map.emplace(ri.get(), offset);
        offset += ri->byte_length();
    }
    *total_len = offset;
    return offset_map;
}

aligned_unique_ptr<char>
writeProgram(RoseEngineBlob &blob, const RoseProgram &program, u32 *total_len) {
    const auto offset_map = makeOffsetMap(program, total_len);
    DEBUG_PRINTF("%zu instructions, len %u\n", program.size(), *total_len);

    auto bytecode = aligned_zmalloc_unique<char>(*total_len);
    char *ptr = bytecode.get();

    for (const auto &ri : program) {
        assert(contains(offset_map, ri.get()));
        const u32 offset = offset_map.at(ri.get());
        ri->write(ptr + offset, blob, offset_map);
    }

    return bytecode;
}

bool RoseProgramEquivalence::operator()(const RoseProgram &prog1,
                                        const RoseProgram &prog2) const {
    if (prog1.size() != prog2.size()) {
        return false;
    }

    u32 len_1 = 0, len_2 = 0;
    const auto offset_map_1 = makeOffsetMap(prog1, &len_1);
    const auto offset_map_2 = makeOffsetMap(prog2, &len_2);

    if (len_1 != len_2) {
        return false;
    }

    auto is_equiv = [&](const unique_ptr<RoseInstruction> &a,
                        const unique_ptr<RoseInstruction> &b) {
        assert(a && b);
        return a->equiv(*b, offset_map_1, offset_map_2);
    };

    return std::equal(prog1.begin(), prog1.end(), prog2.begin(), is_equiv);
}

} // namespace ue2
