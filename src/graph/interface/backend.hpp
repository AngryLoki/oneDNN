/*******************************************************************************
 * Copyright 2020-2022 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#ifndef GRAPH_INTERFACE_BACKEND_HPP
#define GRAPH_INTERFACE_BACKEND_HPP

#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>

#include "graph/interface/partition.hpp"
#include "graph/interface/tensor.hpp"

#define BACKEND_ID_LENGTH 4
#define MAX_BACKEND_NUMS (1 << BACKEND_ID_LENGTH)
#define RESERVED_BACKEND_ID 0 // reserved but not used now

namespace dnnl {
namespace impl {
namespace graph {

// forward declaration
// void register_dnnl_backend();
// void register_fake_backend();

class backend {
public:
    backend(const std::string &name, float priority)
        : name_(name), priority_(priority), id_(get_counter()) {}

    virtual ~backend() = default;

    const std::string &get_name() const { return name_; };
    size_t get_id() const { return id_; }
    float get_priority() const { return priority_; }

    /// Return the physical memory size of the buffer described by the passed
    /// logical tensor
    /// @param lt The logical tensor to get memory size. If it's layout_type
    ///     is opaque, then it's layout id must be generated by this backend.
    ///     This should be guaranteed by frontend
    /// @return The memory size
    virtual size_t get_mem_size(const logical_tensor_t &lt) const = 0;

    /// Check whether two logical tensor is similar (similar means two
    /// logical tensors can be converted to same backend md)
    /// @param lhs
    /// @param rhs
    /// @return true or false
    /// @note This is a default implementation. It regards two logical
    ///     tensors as similar if they are equal bit by bit except their
    ///     ids. Each backend can override this method to provide specific
    ///     check.
    virtual bool compare_logical_tensor(
            const logical_tensor_t &lhs, const logical_tensor_t &rhs) const {
        bool equal = (lhs.ndims == rhs.ndims)
                && (lhs.data_type == rhs.data_type)
                && (lhs.layout_type == rhs.layout_type);

        if (!equal) return false;
        if (lhs.ndims == 0 || lhs.ndims == -1) return true;

        // check dims
        equal = std::equal(std::begin(lhs.dims),
                std::begin(lhs.dims) + lhs.ndims, std::begin(rhs.dims));
        if (!equal) return false;

        // check layout information
        if (lhs.layout_type == layout_type::strided) {
            return std::equal(std::begin(lhs.layout.strides),
                    std::begin(lhs.layout.strides) + lhs.ndims,
                    std::begin(rhs.layout.strides));
        } else if (lhs.layout_type == layout_type::opaque) {
            return lhs.layout.layout_id == rhs.layout.layout_id;
        } else {
            return true;
        }
    }

    /// Run pass on the given graph and generate backend specific
    /// partition_impl objects, which will be stored on the graph
    /// temporarily
    /// @param agraph The graph to be partitioned
    /// @param policy The partition policy
    /// @return The status code
    virtual status_t get_partitions(graph_t &agraph, partition_policy_t policy)
            = 0;

    /// Register the pointer of created backend instance to oneDNN Graph
    static backend *register_backend(const backend *abackend);

private:
    static size_t get_counter() {
        static std::atomic<size_t> counter {RESERVED_BACKEND_ID + 1};
        size_t ret = counter;
        counter++;
        return ret;
    }

    std::string name_;
    float priority_;
    size_t id_;
};

class backend_registry_t {
public:
    static backend_registry_t &get_singleton() {
        static backend_registry_t inst;
        return inst;
    }

    // Will be used in backend class's @register_backend method
    backend *register_backend(const backend *abackend) {
        auto has_colliding_name = [&](const backend *backend) {
            return backend->get_name().compare(abackend->get_name()) == 0;
        };
        auto backend_already_registered = [&]() {
            return std::find_if(sorted_backends_.begin(),
                           sorted_backends_.end(), has_colliding_name)
                    != sorted_backends_.end();
        };

        auto compare_priority = [](const backend *l, const backend *r) {
            return l->get_priority() > r->get_priority();
        };

        if (backend_already_registered()) {
            throw std::runtime_error(
                    "backend name not unique: " + abackend->get_name());
        }

        std::lock_guard<std::mutex> lock(m_);

        backends_[abackend->get_id()] = abackend;
        sorted_backends_.emplace_back(abackend);
        std::sort(sorted_backends_.begin(), sorted_backends_.end(),
                compare_priority);
        return const_cast<backend *>(abackend);
    }

    // This interface will firstly register all available backends and then
    // return sorted backends. The highest priority backend will be at the front
    // of vector
    std::vector<const backend *> &get_registered_backends() {
        invoke_backend_registration();
        std::lock_guard<std::mutex> lock(m_);
        return sorted_backends_;
    }

    // This interface will also try to register all available backends.
    // In order to use get_mem_size() API, we need to dispatch to specific
    // backend according to the backend specific layout id.
    // In this function, we will first decode the layout id to a backend id
    // and a native layout id. Then we will use the backend id to get the
    // backend from the backend registry
    const backend *get_registered_backend(size_t layout_id) {
        invoke_backend_registration();
        size_t backend_id = extract_backend_id(layout_id);
        std::lock_guard<std::mutex> lock(m_);
        return backends_[backend_id];
    }

    static std::pair<size_t, size_t> decode_layout_id(size_t layout_id);

    static size_t encode_layout_id(size_t layout_idx, size_t backend_id);

    static size_t extract_layout_id(size_t layout_id);

    static size_t extract_backend_id(size_t layout_id);

private:
    backend_registry_t() = default;
    backend_registry_t(const backend_registry_t &) = delete;
    backend_registry_t(backend_registry_t &&) = delete;
    backend_registry_t &operator=(const backend_registry_t &) = delete;
    backend_registry_t &operator=(backend_registry_t &&) = delete;

    inline void invoke_backend_registration() {
        std::call_once(register_flag_, []() {
            // register_dnnl_backend();
            // register_fake_backend();
        });
    }

    std::mutex m_;

    std::once_flag register_flag_;

    // sorted backends by priority
    std::vector<const backend *> sorted_backends_;

    // the map from backend id to backend shared pointer
    std::unordered_map<size_t, const backend *> backends_;
};

// Backend API used by each backend to check the constant tensor cache enabling
// status.
bool is_constant_cache_enabled();

} // namespace graph
} // namespace impl
} // namespace dnnl
#endif
