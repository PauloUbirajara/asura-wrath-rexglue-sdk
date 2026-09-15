#pragma once
/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <queue>
#include <set>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <rex/graphics/pipeline/shader/spirv_translator.h>
#include <rex/hash.h>
#include <rex/platform.h>
#include <rex/thread.h>
#include <rex/graphics/primitive_processor.h>
#include <rex/graphics/register_file.h>
#include <rex/graphics/registers.h>
#include <rex/graphics/vulkan/render_target_cache.h>
#include <rex/graphics/vulkan/shader.h>
#include <rex/graphics/xenos.h>
#include <rex/ui/vulkan/api.h>

namespace rex::graphics::vulkan {

class VulkanCommandProcessor;

// TODO(Triang3l): Create a common base for both the Vulkan and the Direct3D
// implementations.
class VulkanPipelineCache {
 public:
  static constexpr size_t kLayoutUIDEmpty = 0;

  class PipelineLayoutProvider {
   public:
    virtual ~PipelineLayoutProvider() {}
    virtual VkPipelineLayout GetPipelineLayout() const = 0;

   protected:
    PipelineLayoutProvider() = default;
  };

  VulkanPipelineCache(VulkanCommandProcessor& command_processor, const RegisterFile& register_file,
                      VulkanRenderTargetCache& render_target_cache,
                      VkShaderStageFlags guest_shader_vertex_stages);
  ~VulkanPipelineCache();

  bool Initialize();
  void Shutdown();
  void InitializeShaderStorage(const std::filesystem::path& cache_root, uint32_t title_id,
                               bool blocking);
  void ShutdownShaderStorage();
  void EndSubmission();

  VulkanShader* LoadShader(xenos::ShaderType shader_type, const uint32_t* host_address,
                           uint32_t dword_count);
  // Analyze shader microcode on the translator thread.
  void AnalyzeShaderUcode(Shader& shader) { shader.AnalyzeUcode(ucode_disasm_buffer_); }

  // Retrieves the shader modification for the current state. The shader must
  // have microcode analyzed.
  SpirvShaderTranslator::Modification GetCurrentVertexShaderModification(
      const Shader& shader, Shader::HostVertexShaderType host_vertex_shader_type,
      uint32_t interpolator_mask, bool ps_param_gen_used) const;
  SpirvShaderTranslator::Modification GetCurrentPixelShaderModification(
      const Shader& shader, uint32_t interpolator_mask, uint32_t param_gen_pos,
      reg::RB_DEPTHCONTROL normalized_depth_control) const;

  bool EnsureShadersTranslated(VulkanShader::VulkanTranslation* vertex_shader,
                               VulkanShader::VulkanTranslation* pixel_shader);
  // TODO(Triang3l): Return a deferred creation handle.
  bool ConfigurePipeline(
      VulkanShader::VulkanTranslation* vertex_shader, VulkanShader::VulkanTranslation* pixel_shader,
      const PrimitiveProcessor::ProcessingResult& primitive_processing_result,
      reg::RB_DEPTHCONTROL normalized_depth_control, uint32_t normalized_color_mask,
      VulkanRenderTargetCache::RenderPassKey render_pass_key, VkPipeline& pipeline_out,
      const PipelineLayoutProvider*& pipeline_layout_out, void** pipeline_handle_out = nullptr);
  bool IsCreatingPipelines() const;
  void GetPipelineAndLayoutByHandle(void* handle, VkPipeline& pipeline_out,
                                    const PipelineLayoutProvider*& pipeline_layout_out,
                                    bool* is_placeholder_out = nullptr) const;

  void EnqueueShaderWrite(const Shader* shader);
  void EnqueuePipelineFlush();

 private:
  REXPACKEDSTRUCT(ShaderStoredHeader, {
    uint64_t ucode_data_hash;

    uint32_t ucode_dword_count : 31;
    xenos::ShaderType type : 1;

    static constexpr uint32_t kVersion = 0x20201219;
  });

  enum class PipelineGeometryShader : uint32_t {
    kNone,
    kPointList,
    kRectangleList,
    kQuadList,
  };

  enum class PipelinePrimitiveTopology : uint32_t {
    kPointList,
    kLineList,
    kLineStrip,
    kTriangleList,
    kTriangleStrip,
    kTriangleFan,
    kLineListWithAdjacency,
    kPatchList,
  };

  enum class PipelinePolygonMode : uint32_t {
    kFill,
    kLine,
    kPoint,
  };

  enum class PipelineBlendFactor : uint32_t {
    kZero,
    kOne,
    kSrcColor,
    kOneMinusSrcColor,
    kDstColor,
    kOneMinusDstColor,
    kSrcAlpha,
    kOneMinusSrcAlpha,
    kDstAlpha,
    kOneMinusDstAlpha,
    kConstantColor,
    kOneMinusConstantColor,
    kConstantAlpha,
    kOneMinusConstantAlpha,
    kSrcAlphaSaturate,
  };

  // Update PipelineDescription::kVersion if anything is changed!
  REXPACKEDSTRUCT(PipelineRenderTarget, {
    PipelineBlendFactor src_color_blend_factor : 4;  // 4
    PipelineBlendFactor dst_color_blend_factor : 4;  // 8
    xenos::BlendOp color_blend_op : 3;               // 11
    PipelineBlendFactor src_alpha_blend_factor : 4;  // 15
    PipelineBlendFactor dst_alpha_blend_factor : 4;  // 19
    xenos::BlendOp alpha_blend_op : 3;               // 22
    uint32_t color_write_mask : 4;                   // 26
  });

  REXPACKEDSTRUCT(PipelineDescription, {
    uint64_t cached_hash;  // Pre-calculated hash stored upon creation
    uint64_t vertex_shader_hash;
    uint64_t vertex_shader_modification;
    uint64_t pixel_shader_hash;
    uint64_t pixel_shader_modification;
    VulkanRenderTargetCache::RenderPassKey render_pass_key;

    // Shader stages & Input assembly...
    PipelineGeometryShader geometry_shader : 2;
    PipelinePrimitiveTopology primitive_topology : 3;
    uint32_t primitive_restart : 1;
    xenos::TessellationMode tessellation_mode : 2;
    uint32_t depth_clamp_enable : 1;
    PipelinePolygonMode polygon_mode : 2;
    uint32_t cull_front : 1;
    uint32_t cull_back : 1;
    uint32_t front_face_clockwise : 1;
    uint32_t rasterizer_discard : 1;

    // Depth / stencil...
    uint32_t depth_write_enable : 1;
    xenos::CompareFunction depth_compare_op : 3;
    uint32_t stencil_test_enable : 1;
    xenos::StencilOp stencil_front_fail_op : 3;
    xenos::StencilOp stencil_front_pass_op : 3;
    xenos::StencilOp stencil_front_depth_fail_op : 3;
    xenos::CompareFunction stencil_front_compare_op : 3;
    xenos::StencilOp stencil_back_fail_op : 3;

    xenos::StencilOp stencil_back_pass_op : 3;
    xenos::StencilOp stencil_back_depth_fail_op : 3;
    xenos::CompareFunction stencil_back_compare_op : 3;
    uint32_t sample_rate_shading : 1;

    PipelineRenderTarget render_targets[xenos::kMaxColorRenderTargets];

    static constexpr uint32_t kVersion = 0x20260228;

    PipelineDescription() {
      Reset();
    }
    PipelineDescription(const PipelineDescription& description) {
      std::memcpy(this, &description, sizeof(*this));
    }
    PipelineDescription& operator=(const PipelineDescription& description) {
      std::memcpy(this, &description, sizeof(*this));
      return *this;
    }
    bool operator==(const PipelineDescription& description) const {
      // Fast-path 64-bit hash comparison before full memory compare
      if (cached_hash != description.cached_hash) {
        return false;
      }
      return std::memcmp(this, &description, sizeof(*this)) == 0;
    }
    void Reset() {
      std::memset(this, 0, sizeof(*this));
    }

    // Finalize description state and calculate static hash once
    void FinalizeHash() {
      cached_hash = 0;  // Exclude hash field itself during XXH3 calculation
      cached_hash = XXH3_64bits(reinterpret_cast<const uint8_t*>(this) + sizeof(cached_hash),
                                sizeof(*this) - sizeof(cached_hash));
    }

    uint64_t GetHash() const {
      return cached_hash;
    }

    struct Hasher {
      size_t operator()(const PipelineDescription& description) const {
        return static_cast<size_t>(description.cached_hash);
      }
    };
  });
  REXPACKEDSTRUCT(PipelineStoredDescription, {
    uint64_t description_hash;
    PipelineDescription description;
  });

  struct Pipeline {
    std::atomic<VkPipeline> pipeline{VK_NULL_HANDLE};
    // The layouts are owned by the VulkanCommandProcessor, and must not be
    // destroyed by it while the pipeline cache is active.
    std::atomic<const PipelineLayoutProvider*> pipeline_layout{nullptr};
    std::atomic<bool> is_placeholder{false};
    Pipeline() = default;
    Pipeline(const PipelineLayoutProvider* pipeline_layout_provider)
        : pipeline_layout(pipeline_layout_provider) {}
    Pipeline(const Pipeline& other)
        : pipeline(other.pipeline.load(std::memory_order_acquire)),
          pipeline_layout(other.pipeline_layout.load(std::memory_order_acquire)),
          creation_failed(other.creation_failed.load(std::memory_order_acquire)),
          is_placeholder(other.is_placeholder.load(std::memory_order_acquire)) {}
    Pipeline& operator=(const Pipeline& other) {
      if (this == &other) {
        return *this;
      }
      pipeline.store(other.pipeline.load(std::memory_order_acquire), std::memory_order_release);
      pipeline_layout.store(other.pipeline_layout.load(std::memory_order_acquire),
                            std::memory_order_release);
      is_placeholder.store(other.is_placeholder.load(std::memory_order_acquire),
                           std::memory_order_release);
      creation_failed.store(other.creation_failed.load(std::memory_order_acquire),
                            std::memory_order_release);
      return *this;
    }

    std::atomic<bool> creation_failed{false};  // Tombstone flag for failed pipelines
  };

  // Description that can be passed from the command processor thread to the
  // creation threads, with everything needed from caches pre-looked-up.
  struct PipelineCreationArguments {
    uint8_t priority = 0;
    std::pair<const PipelineDescription, Pipeline>* pipeline = nullptr;
    const PipelineLayoutProvider* pipeline_layout = nullptr;
    // Guest shader translation (VS or TES depending on host vertex type).
    const VulkanShader::VulkanTranslation* vertex_shader = nullptr;
    const VulkanShader::VulkanTranslation* pixel_shader = nullptr;
    // Non-guest stages for tessellation.
    VkShaderModule tessellation_vertex_shader = VK_NULL_HANDLE;
    VkShaderModule tessellation_control_shader = VK_NULL_HANDLE;
    uint32_t tessellation_patch_control_points = 0;
    VkShaderModule geometry_shader = VK_NULL_HANDLE;
    // VK_NULL_HANDLE when dynamic rendering is used.
    VkRenderPass render_pass = VK_NULL_HANDLE;
  };
  struct PipelineCreationArgumentsPriorityComparator {
    bool operator()(const PipelineCreationArguments& a, const PipelineCreationArguments& b) const {
      return a.priority < b.priority;
    }
  };

  union TessellationControlShaderKey {
    uint32_t key;
    struct {
      Shader::HostVertexShaderType host_vertex_shader_type : Shader::kHostVertexShaderTypeBitCount;
      xenos::TessellationMode tessellation_mode : 2;
    };

    TessellationControlShaderKey() : key(0) { static_assert_size(*this, sizeof(key)); }

    struct Hasher {
      size_t operator()(const TessellationControlShaderKey& key) const {
        return std::hash<uint32_t>{}(key.key);
      }
    };
    bool operator==(const TessellationControlShaderKey& other_key) const {
      return key == other_key.key;
    }
  };

  union GeometryShaderKey {
    uint32_t key;
    struct {
      PipelineGeometryShader type : 2;
      uint32_t interpolator_count : 5;
      uint32_t user_clip_plane_count : 3;
      uint32_t user_clip_plane_cull : 1;
      uint32_t has_vertex_kill_and : 1;
      uint32_t has_point_size : 1;
      uint32_t has_point_coordinates : 1;
      // PA_CL_CLIP_CNTL::ps_ucp_mode for point primitives.
      uint32_t point_ps_ucp_mode : 2;
    };

    GeometryShaderKey() : key(0) { static_assert_size(*this, sizeof(key)); }

    struct Hasher {
      size_t operator()(const GeometryShaderKey& key) const {
        return std::hash<uint32_t>{}(key.key);
      }
    };
    bool operator==(const GeometryShaderKey& other_key) const { return key == other_key.key; }
    bool operator!=(const GeometryShaderKey& other_key) const { return !(*this == other_key); }
  };

  VulkanShader* LoadShader(xenos::ShaderType shader_type, const uint32_t* host_address,
                           uint32_t dword_count, uint64_t data_hash);

  // Can be called from multiple threads.
  bool TranslateAnalyzedShader(SpirvShaderTranslator& translator,
                               VulkanShader::VulkanTranslation& translation);

  void WritePipelineRenderTargetDescription(reg::RB_BLENDCONTROL blend_control, uint32_t write_mask,
                                            PipelineRenderTarget& render_target_out) const;
  bool GetCurrentStateDescription(
      const VulkanShader::VulkanTranslation* vertex_shader,
      const VulkanShader::VulkanTranslation* pixel_shader,
      const PrimitiveProcessor::ProcessingResult& primitive_processing_result,
      reg::RB_DEPTHCONTROL normalized_depth_control, uint32_t normalized_color_mask,
      VulkanRenderTargetCache::RenderPassKey render_pass_key,
      PipelineDescription& description_out) const;

  // Whether the pipeline for the given description is supported by the device.
  bool ArePipelineRequirementsMet(const PipelineDescription& description) const;

  static bool GetGeometryShaderKey(PipelineGeometryShader geometry_shader_type,
                                   SpirvShaderTranslator::Modification vertex_shader_modification,
                                   SpirvShaderTranslator::Modification pixel_shader_modification,
                                   GeometryShaderKey& key_out);
  static uint32_t GetTessellationPatchControlPointCount(
      Shader::HostVertexShaderType host_vertex_shader_type,
      xenos::TessellationMode tessellation_mode);
  VkShaderModule GetTessellationVertexShader(bool adaptive);
  VkShaderModule GetTessellationControlShader(Shader::HostVertexShaderType host_vertex_shader_type,
                                              xenos::TessellationMode tessellation_mode);
  VkShaderModule GetGeometryShader(GeometryShaderKey key);
  bool TryGetPipelineCreationArgumentsForDescription(
      const PipelineDescription& description,
      std::pair<const PipelineDescription, Pipeline>* pipeline,
      PipelineCreationArguments& creation_arguments, bool for_placeholder = false);

  // Can be called from creation threads - all needed data must be fully set up
  // at the point of the call: shaders must be translated, and the pipeline
  // layout and render pass object (unless dynamic rendering is used) must be
  // available.
  bool EnsurePipelineCreated(const PipelineCreationArguments& creation_arguments,
                             VkShaderModule fragment_shader_override = VK_NULL_HANDLE);
  void CreationThread(size_t thread_index);
  void CreateQueuedPipelinesOnProcessorThread();
  void ProcessDeferredPipelineDestructions(bool force_all);

  REXPACKEDSTRUCT(PipelineStorageHeader, {
    static constexpr uint32_t kMagic = 0x58454E4F;  // 'XENO'
    static constexpr uint32_t kVersion = 1;

    uint32_t magic;
    uint32_t version;
    uint8_t pipeline_cache_uuid[VK_UUID_SIZE];
    uint32_t vendor_id;
    uint32_t device_id;
    uint64_t shader_count;
    uint64_t pipeline_count;
  });

  bool ValidateDriverCacheHeader(const PipelineStorageHeader& header);

  VulkanCommandProcessor& command_processor_;
  const RegisterFile& register_file_;
  VulkanRenderTargetCache& render_target_cache_;
  VkShaderStageFlags guest_shader_vertex_stages_;

  // Temporary storage for AnalyzeUcode calls on the processor thread.
  string::StringBuffer ucode_disasm_buffer_;
  // Reusable shader translator on the command processor thread.
  std::unique_ptr<SpirvShaderTranslator> shader_translator_;

  struct LayoutUID {
    size_t uid;
    size_t vector_span_offset;
    size_t vector_span_length;
  };
  std::mutex layouts_mutex_;
  // Texture binding layouts of different shaders, for obtaining layout UIDs.
  std::vector<VulkanShader::TextureBinding> texture_binding_layouts_;
  // Map of texture binding layouts used by shaders, for obtaining UIDs. Keys
  // are XXH3 hashes of layouts, values need manual collision resolution using
  // layout_vector_offset:layout_length of texture_binding_layouts_.
  std::unordered_multimap<uint64_t, LayoutUID, rex::IdentityHasher<uint64_t>>
      texture_binding_layout_map_;

  // Ucode hash -> shader.
  std::unordered_map<uint64_t, VulkanShader*, rex::IdentityHasher<uint64_t>> shaders_;

  // Geometry shaders for Xenos primitive types not supported by Vulkan.
  // Stores VK_NULL_HANDLE if failed to create.
  std::unordered_map<GeometryShaderKey, VkShaderModule, GeometryShaderKey::Hasher>
      geometry_shaders_;

  // Fixed-function emulation shaders for tessellation.
  bool tessellation_indexed_vertex_shader_attempted_ = false;
  VkShaderModule tessellation_indexed_vertex_shader_ = VK_NULL_HANDLE;
  bool tessellation_adaptive_vertex_shader_attempted_ = false;
  VkShaderModule tessellation_adaptive_vertex_shader_ = VK_NULL_HANDLE;
  std::unordered_map<TessellationControlShaderKey, VkShaderModule,
                     TessellationControlShaderKey::Hasher>
      tessellation_control_shaders_;

  // Empty depth-only pixel shader for writing to depth buffer using fragment
  // shader interlock when no Xenos pixel shader provided.
  VkShaderModule depth_only_fragment_shader_ = VK_NULL_HANDLE;
  VkShaderModule placeholder_pixel_shader_ = VK_NULL_HANDLE;
  // Depth-only shaders for float24 emulation when no Xenos pixel shader is
  // provided in host render target mode.
  VkShaderModule depth_float24_truncate_fragment_shader_ = VK_NULL_HANDLE;
  VkShaderModule depth_float24_round_fragment_shader_ = VK_NULL_HANDLE;

  std::unordered_map<PipelineDescription, Pipeline, PipelineDescription::Hasher> pipelines_;
  mutable std::shared_mutex pipelines_mutex_;

  // Previously used pipeline, to avoid lookups if the state wasn't changed.
  const std::pair<const PipelineDescription, Pipeline>* last_pipeline_ = nullptr;
  // <Submission index, pipeline>.
  std::deque<std::pair<uint64_t, VkPipeline>> deferred_destroy_pipelines_;
  std::mutex deferred_destroy_lock_;

  // Currently open shader storage path.
  std::filesystem::path shader_storage_cache_root_;
  uint32_t shader_storage_title_id_ = 0;

  // Shader storage output stream, for preload in the next emulator runs.
  FILE* shader_storage_file_ = nullptr;
  // For only writing shaders to the currently open storage once, incremented
  // when switching the storage.
  uint32_t shader_storage_index_ = 0;
  bool shader_storage_file_flush_needed_ = false;

  // Pipeline storage output stream, for preload in the next emulator runs.
  FILE* pipeline_storage_file_ = nullptr;
  bool pipeline_storage_file_flush_needed_ = false;

  // Thread for asynchronous writing to the storage streams.
  void StorageWriteThread();
  std::mutex storage_write_request_lock_;
  std::condition_variable storage_write_request_cond_;
  // Storage thread input is protected with storage_write_request_lock_, and the
  // thread is notified about its change via storage_write_request_cond_.
  std::deque<const Shader*> storage_write_shader_queue_;
  std::deque<PipelineStoredDescription> storage_write_pipeline_queue_;
  bool storage_write_flush_shaders_ = false;
  bool storage_write_flush_pipelines_ = false;
  bool storage_write_thread_shutdown_ = false;
  std::unique_ptr<rex::thread::Thread> storage_write_thread_;

  mutable std::mutex creation_request_lock_;
  std::condition_variable creation_request_cond_;
  std::priority_queue<PipelineCreationArguments, std::vector<PipelineCreationArguments>,
                      PipelineCreationArgumentsPriorityComparator>
      creation_queue_;
  size_t creation_threads_busy_ = 0;
  bool startup_loading_ = false;
  std::unique_ptr<rex::thread::Event> creation_completion_event_;
  bool creation_completion_set_event_ = false;
  size_t creation_threads_shutdown_from_ = SIZE_MAX;
  std::vector<std::unique_ptr<rex::thread::Thread>> creation_threads_;

  VkPipelineCache vulkan_pipeline_cache_ = VK_NULL_HANDLE;

  struct PipelineInputStateKey {
    const VulkanShader::VulkanTranslation* vertex_shader = nullptr;
    const VulkanShader::VulkanTranslation* pixel_shader = nullptr;
    PrimitiveProcessor::ProcessingResult primitive_result = {};
    uint32_t depth_control = 0;
    uint32_t color_mask = 0;
    uint64_t render_pass_key = 0;
    uint32_t pa_su_sc_mode_cntl = 0;
    uint32_t pa_cl_clip_cntl = 0;
    uint32_t rb_blendcontrol[4] = {};

    bool operator==(const PipelineInputStateKey& other) const {
      return std::memcmp(this, &other, sizeof(PipelineInputStateKey)) == 0;
    }
    bool operator!=(const PipelineInputStateKey& other) const { return !(*this == other); }
  };

  mutable PipelineInputStateKey last_input_state_key_ = {};
};

}  // namespace rex::graphics::vulkan
