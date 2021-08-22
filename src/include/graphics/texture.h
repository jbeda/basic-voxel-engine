#pragma once
namespace bve {
    namespace graphics {
        class texture : public ref_counted {
        public:
            // Load a texture from path into data. Return true if successful
            static bool load_image(const std::filesystem::path& path, std::vector<uint8_t>& data, int32_t& width, int32_t& height, int32_t& channels);
            texture() = default;
            virtual ~texture() = default;
            texture(const texture&) = delete;
            texture& operator=(const texture&) = delete;
            virtual void bind(GLint slot = 0) = 0;
            virtual glm::ivec2 get_size() = 0;
            virtual int32_t get_channels() = 0;
            virtual void* get_id() = 0;
        };
    }
}