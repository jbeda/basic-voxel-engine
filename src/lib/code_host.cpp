#include "bve_pch.h"
#include "code_host.h"
#include "script_wrappers.h"
namespace bve {
    static void parse_name(const std::string& full_name, std::string& namespace_name, std::string& class_name) {
        size_t period_position = full_name.find_last_of('.');
        if (period_position == std::string::npos) {
            namespace_name.clear();
            class_name = full_name;
        } else {
            namespace_name = full_name.substr(0, period_position);
            class_name = full_name.substr(period_position + 1);
        }
    }
    namespace managed {
        wrapper::wrapper(MonoDomain* domain) {
            this->m_domain = domain;
        }
        MonoDomain* wrapper::get_domain() {
            return this->m_domain;
        }
        void object::handle_exception(ref<object> exception) {
            ref<class_> object_class = class_::get_class(exception);
            ref<class_> exception_class = ref<class_>::create(mono_get_exception_class(), exception->get_domain());
            auto property = exception_class->get_property("Message");
            std::string message = exception->get(property)->get_string();
            property = exception_class->get_property("StackTrace");
            ref<object> stacktrace_object = exception->get(property);
            spdlog::error("[managed object] threw {0}: {1}", object_class->get_name(), message);
            if (stacktrace_object) {
                std::string stacktrace = stacktrace_object->get_string();
                std::stringstream ss(stacktrace);
                std::string line;
                while (std::getline(ss, line)) {
                    spdlog::error("[managed object] {0}", line);
                }
            }
            property = exception_class->get_property("InnerException");
            ref<object> inner_exception = exception->get(property);
            if (inner_exception) {
                spdlog::info("[managed object] inner exception:");
                handle_exception(inner_exception);
            }
        }
        object::object(MonoObject* _object, MonoDomain* domain) : wrapper(domain) {
            this->m_handle = mono_gchandle_new(_object, false);
        }
        object::object(const std::string& string, MonoDomain* domain) : wrapper(domain) {
            MonoString* str = mono_string_new(domain, string.c_str());
            this->m_handle = mono_gchandle_new((MonoObject*)str, false);
        }
        object::~object() {
            mono_gchandle_free(this->m_handle);
        }
        ref<object> object::get(MonoClassField* field) {
            MonoObject* _object = mono_gchandle_get_target(this->m_handle);
            MonoObject* value = mono_field_get_value_object(this->get_domain(), field, _object);
            ref<object> returned_object;
            if (value) {
                returned_object = ref<object>::create(value, this->get_domain());
            }
            return returned_object;
        }
        ref<object> object::get(MonoProperty* property) {
            MonoObject* _object = mono_gchandle_get_target(this->m_handle);
            MonoObject* exc = nullptr;
            MonoObject* value = mono_property_get_value(property, _object, nullptr, &exc);
            if (exc) {
                ref<object> exception = ref<object>::create(exc, this->get_domain());
                handle_exception(exception);
            }
            ref<object> returned_object;
            if (value) {
                returned_object = ref<object>::create(value, this->get_domain());
            }
            return returned_object;
        }
        void object::set(MonoClassField* field, void* value) {
            MonoObject* _object = mono_gchandle_get_target(this->m_handle);
            mono_field_set_value(_object, field, value);
        }
        void object::set(MonoProperty* property, void* value) {
            MonoObject* _object = mono_gchandle_get_target(this->m_handle);
            MonoObject* exc = nullptr;
            mono_property_set_value(property, _object, nullptr, &exc);
            if (exc) {
                ref<object> exception = ref<object>::create(exc, this->get_domain());
                handle_exception(exception);
            }
        }
        std::string object::get_string() {
            MonoObject* _object = mono_gchandle_get_target(this->m_handle);
            MonoString* string = (MonoString*)_object;
            return std::string(mono_string_to_utf8(string));
        }
        void* object::get() {
            return mono_gchandle_get_target(this->m_handle);
        }
        MonoImage* object::get_image() {
            MonoObject* _object = mono_gchandle_get_target(this->m_handle);
            MonoClass* _class = mono_object_get_class(_object);
            return mono_class_get_image(_class);
        }
        ref<class_> class_::get_class(ref<object> _object) {
            auto object_ptr = (MonoObject*)_object->get();
            MonoClass* class_ptr = mono_object_get_class(object_ptr);
            return ref<class_>::create(class_ptr, _object->get_domain());
        }
        class_::class_(MonoClass* _class, MonoDomain* domain) : wrapper(domain) {
            this->m_class = _class;
        }
        std::string class_::get_name() {
            std::string namespace_name, class_name;
            this->get_name(namespace_name, class_name);
            return namespace_name + "." + class_name;
        }
        void class_::get_name(std::string& namespace_name, std::string& class_name) {
            namespace_name = mono_class_get_namespace(this->m_class);
            class_name = mono_class_get_name(this->m_class);
        }
        MonoMethod* class_::get_method(const std::string& descriptor) {
            MonoMethodDesc* desc = mono_method_desc_new(descriptor.c_str(), false);
            return mono_method_desc_search_in_class(desc, this->m_class);
        }
        MonoClassField* class_::get_field(const std::string& name) {
            return mono_class_get_field_from_name(this->m_class, name.c_str());
        }
        MonoProperty* class_::get_property(const std::string& name) {
            return mono_class_get_property_from_name(this->m_class, name.c_str());
        }
        void* class_::get() {
            return this->m_class;
        }
        MonoImage* class_::get_image() {
            return mono_class_get_image(this->m_class);
        }
        ref<type> type::get_type(ref<class_> _class) {
            MonoClass* _class_ = (MonoClass*)_class->get();
            MonoType* _type = mono_class_get_type(_class_);
            return ref<type>::create(_type, _class->get_domain());
        }
        type::type(MonoType* _type, MonoDomain* domain) : wrapper(domain) {
            this->m_type = _type;
        }
        type::type(MonoReflectionType* _type, MonoDomain* domain) : wrapper(domain) {
            this->m_type = mono_reflection_type_get_type(_type);
        }
        ref<class_> type::get_class() {
            MonoClass* _class = mono_type_get_class(this->m_type);
            return ref<class_>::create(_class, this->get_domain());
        }
        void* type::get() {
            return this->m_type;
        }
        MonoImage* type::get_image() {
            MonoClass* _class = mono_type_get_class(this->m_type);
            return mono_class_get_image(_class);
        }
        ref<assembly> assembly::get_assembly(ref<wrapper> wrapper_object) {
            MonoImage* image = wrapper_object->get_image();
            MonoAssembly* _assembly = mono_image_get_assembly(image);
            MonoDomain* domain = wrapper_object->get_domain();
            return ref<assembly>::create(_assembly, domain);
        }
        assembly::assembly(MonoAssembly* _assembly, MonoDomain* domain) : wrapper(domain) {
            this->m_assembly = _assembly;
            this->m_image = mono_assembly_get_image(this->m_assembly);
        }
        ref<class_> assembly::get_class(const std::string& name) {
            std::string namespace_name, class_name;
            parse_name(name, namespace_name, class_name);
            return this->get_class(namespace_name, class_name);
        }
        ref<class_> assembly::get_class(const std::string& namespace_name, const std::string& class_name) {
            MonoClass* _class = mono_class_from_name(this->m_image, namespace_name.c_str(), class_name.c_str());
            return ref<class_>::create(_class, this->get_domain());
        }
        MonoMethod* assembly::get_method(const std::string& descriptor) {
            MonoMethodDesc* desc = mono_method_desc_new(descriptor.c_str(), false);
            return mono_method_desc_search_in_image(desc, this->m_image);
        }
        void* assembly::get() {
            return this->m_assembly;
        }
        MonoImage* assembly::get_image() {
            return this->m_image;
        }
    }
    std::unordered_map<std::string, void*> code_host::get_script_wrappers() {
        using namespace script_wrappers;
        // todo: improve this macro
#define register(name, function) { name, (void*)function },
        return {
            register("BasicVoxelEngine.Application::GetDeltaTime", BasicVoxelEngine_Application_GetDeltaTime)

            register("BasicVoxelEngine.Logger::PrintDebug_Native", BasicVoxelEngine_Logger_PrintDebug)
            register("BasicVoxelEngine.Logger::PrintInfo_Native", BasicVoxelEngine_Logger_PrintInfo)
            register("BasicVoxelEngine.Logger::PrintWarning_Native", BasicVoxelEngine_Logger_PrintWarning)
            register("BasicVoxelEngine.Logger::PrintError_Native", BasicVoxelEngine_Logger_PrintError)

            register("BasicVoxelEngine.Registry::RegistryTypes_Native", BasicVoxelEngine_Registry_RegisterTypes)
            register("BasicVoxelEngine.Registry::RegisterExists_Native", BasicVoxelEngine_Registry_RegisterExists)
            register("BasicVoxelEngine.Registry::CreateRegisterRef_Native", BasicVoxelEngine_Registry_CreateRegisterRef)

            register("BasicVoxelEngine.Register`1::CreateRef_Native", BasicVoxelEngine_Register_CreateRef)
            register("BasicVoxelEngine.Register`1::GetCount_Native", BasicVoxelEngine_Register_GetCount)
            register("BasicVoxelEngine.Register`1::GetIndex_Native", BasicVoxelEngine_Register_GetIndex)
            register("BasicVoxelEngine.Register`1::GetNamespacedName_Native", BasicVoxelEngine_Register_GetNamespacedName)
            register("BasicVoxelEngine.Register`1::RegisterObject_Native", BasicVoxelEngine_Register_RegisterObject)
            register("BasicVoxelEngine.Register`1::IsManaged_Native", BasicVoxelEngine_Register_IsManaged)
            register("BasicVoxelEngine.Register`1::GetManagedObject_Native", BasicVoxelEngine_Register_GetManagedObject)

            register("BasicVoxelEngine.RegisteredObject`1::DestroyRef_Native", BasicVoxelEngine_RegisteredObject_DestroyRef)

            register("BasicVoxelEngine.Block::GetOpacity_Native", BasicVoxelEngine_Block_GetOpacity)
            register("BasicVoxelEngine.Block::GetSolid_Native", BasicVoxelEngine_Block_GetSolid)
            register("BasicVoxelEngine.Block::GetFriendlyName_Native", BasicVoxelEngine_Block_GetFriendlyName)
        };
#undef register
    }
    code_host::code_host() {
        mono_config_parse(nullptr);
        mono_set_assemblies_path(MONO_ASSEMBLIES);
        this->m_domain = mono_jit_init(BVE_TARGET_NAME);
    }
    code_host::~code_host() {
        mono_jit_cleanup(this->m_domain);
    }
    MonoDomain* code_host::get_domain() {
        return this->m_domain;
    }
    void code_host::load_assembly(const std::filesystem::path& path) {
        std::string string_path = path.string();
        // old-fashioned c-style file reading
        FILE* f = fopen(string_path.c_str(), "rb");
        if (!f) {
            throw std::runtime_error("[code host] could not open assembly");
        }
        fseek(f, 0, SEEK_END);
        size_t file_size = (size_t)ftell(f);
        rewind(f);
        char* file_data = (char*)malloc(file_size * sizeof(char));
        if (!file_data) {
            fclose(f);
            throw std::runtime_error("[code host] application ran out of memory");
        }
        memset(file_data, 0, file_size * sizeof(char));
        size_t bytes_read = fread(file_data, sizeof(char), file_size, f);
        if (bytes_read != file_size) {
            fclose(f);
            free(file_data);
            throw std::runtime_error("[code host] could not read entire assembly");
        }
        fclose(f);
        MonoImageOpenStatus status;
        MonoImage* image = mono_image_open_from_data_full(file_data, file_size * sizeof(char), true, &status, false);
        free(file_data);
        if (status != MONO_IMAGE_OK) {
            return;
        }
        MonoAssembly* assembly = mono_assembly_load_from_full(image, string_path.c_str(), &status, false);
        mono_image_close(image);
        this->m_assemblies.push_back(ref<managed::assembly>::create(assembly, this->m_domain));
    }
    std::vector<ref<managed::assembly>> code_host::get_loaded_assemblies() {
        return this->m_assemblies;
    }
    ref<managed::class_> code_host::find_class(const std::string& name) {
        std::string namespace_name, class_name;
        parse_name(name, namespace_name, class_name);
        return this->find_class(namespace_name, class_name);
    }
    ref<managed::class_> code_host::find_class(const std::string& namespace_name, const std::string& class_name) {
        for (auto assembly : this->m_assemblies) {
            ref<managed::class_> current_class = assembly->get_class(namespace_name, class_name);
            if (current_class) {
                return current_class;
            }
        }
        return nullptr;
    }
}