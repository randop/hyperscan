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

#ifndef ROSE_BUILD_ENGINE_BLOB_H
#define ROSE_BUILD_ENGINE_BLOB_H

#include "rose_internal.h"

#include "ue2common.h"
#include "util/alloc.h"
#include "util/container.h"
#include "util/verify_types.h"

#include <vector>
#include <type_traits>

#include <boost/core/noncopyable.hpp>

namespace ue2 {

class RoseEngineBlob : boost::noncopyable {
public:
    /** \brief Base offset of engine_blob in the Rose engine bytecode. */
    static constexpr u32 base_offset = ROUNDUP_CL(sizeof(RoseEngine));

    bool empty() const {
        return blob.empty();
    }

    size_t size() const {
        return blob.size();
    }

    const char *data() const {
        return blob.data();
    }

    u32 add(const void *a, const size_t len, const size_t align) {
        pad(align);

        size_t rv = base_offset + blob.size();
        assert(rv >= base_offset);
        DEBUG_PRINTF("write %zu bytes at offset %zu\n", len, rv);

        assert(ISALIGNED_N(blob.size(), align));

        blob.resize(blob.size() + len);
        memcpy(&blob.back() - len + 1, a, len);

        return verify_u32(rv);
    }

    template<typename T>
    u32 add(const T &a) {
        static_assert(std::is_pod<T>::value, "should be pod");
        return add(&a, sizeof(a), alignof(T));
    }

    template<typename T>
    u32 add(const T &a, const size_t len) {
        static_assert(std::is_pod<T>::value, "should be pod");
        return add(&a, len, alignof(T));
    }

    template<typename Iter>
    u32 add(Iter b, const Iter &e) {
        using value_type = typename std::iterator_traits<Iter>::value_type;
        static_assert(std::is_pod<value_type>::value, "should be pod");

        if (b == e) {
            return 0;
        }

        u32 offset = add(*b);
        for (++b; b != e; ++b) {
            add(*b);
        }

        return offset;
    }

    void write_bytes(RoseEngine *engine) {
        copy_bytes((char *)engine + base_offset, blob);
    }

private:
    void pad(size_t align) {
        assert(ISALIGNED_N(base_offset, align));
        size_t s = blob.size();

        if (ISALIGNED_N(s, align)) {
            return;
        }

        blob.resize(s + align - s % align);
    }

    /**
     * \brief Contents of the Rose bytecode immediately following the
     * RoseEngine.
     */
    std::vector<char, AlignedAllocator<char, 64>> blob;
};

} // namespace ue2

#endif // ROSE_BUILD_ENGINE_BLOB_H
