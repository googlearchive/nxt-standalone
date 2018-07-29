//* Copyright 2017 The Dawn Authors
//*
//* Licensed under the Apache License, Version 2.0 (the "License");
//* you may not use this file except in compliance with the License.
//* You may obtain a copy of the License at
//*
//*     http://www.apache.org/licenses/LICENSE-2.0
//*
//* Unless required by applicable law or agreed to in writing, software
//* distributed under the License is distributed on an "AS IS" BASIS,
//* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//* See the License for the specific language governing permissions and
//* limitations under the License.

#include "dawn/dawn.h"
#include "dawn/dawncpp.h"

#include "common/Assert.h"

#include "dawn_native/ErrorData.h"
#include "dawn_native/ValidationUtils_autogen.h"
#include "dawn_native/{{namespace}}/GeneratedCodeIncludes.h"

namespace dawn_native {
namespace {{namespace}} {

    namespace {

        {% set methodsWithExtraValidation = (
            "CommandBufferBuilderGetResult",
            "QueueSubmit",
        ) %}

        {% for type in by_category["object"] %}
            {% for method in native_methods(type) %}
                {% set suffix = as_MethodSuffix(type.name, method.name) %}

                //* Entry point without validation, forwards the arguments to the method directly
                {{as_backendType(method.return_type)}} NonValidating{{suffix}}(
                    {{-as_backendType(type)}} self
                    {%- for arg in method.arguments -%}
                        , {{as_annotated_backendType(arg)}}
                    {%- endfor -%}
                ) {
                    {% if method.return_type.name.canonical_case() != "void" %}
                        auto result =
                    {%- endif %}
                    self->{{method.name.CamelCase()}}(
                        {%- for arg in method.arguments -%}
                            {%- if not loop.first %}, {% endif -%}
                            {%- if arg.type.category in ["enum", "bitmask"] -%}
                                static_cast<dawn::{{as_cppType(arg.type.name)}}>({{as_varName(arg.name)}})
                            {%- elif arg.type.category == "structure" and arg.annotation != "value" -%}
                                reinterpret_cast<const dawn::{{as_cppType(arg.type.name)}}*>({{as_varName(arg.name)}})
                            {%- else -%}
                                {{as_varName(arg.name)}}
                            {%- endif -%}
                        {%- endfor -%}
                    );
                    {% if method.return_type.name.canonical_case() != "void" %}
                        return reinterpret_cast<{{as_backendType(method.return_type)}}>(result);
                    {% endif %}
                }

                //* Autogenerated part of the entry point validation
                //*  - Check that enum and bitmaks are in the correct range
                //*  - Check that builders have not been consumed already
                //*  - Others TODO
                bool ValidateBase{{suffix}}(
                    {{-as_backendType(type)}} self
                    {%- for arg in method.arguments -%}
                        , {{as_annotated_backendType(arg)}}
                    {%- endfor -%}
                ) {
                    {% if type.is_builder and method.name.canonical_case() not in ("release", "reference") %}
                        if (!self->CanBeUsed()) {
                            self->GetDevice()->HandleError("Builder cannot be used after GetResult");
                            return false;
                        }
                    {% else %}
                        (void) self;
                    {% endif %}
                    bool error = false;
                    {% for arg in method.arguments %}
                        {% set cppType = as_cppType(arg.type.name) %}
                        {% set argName = as_varName(arg.name) %}
                        {% if arg.type.category in ["enum", "bitmask"] %}
                            MaybeError {{argName}}Valid = Validate{{cppType}}(static_cast<dawn::{{cppType}}>({{argName}}));
                            if ({{argName}}Valid.IsError()) {
                                delete {{argName}}Valid.AcquireError();
                                error = true;
                            }
                        {% else %}
                            (void) {{argName}};
                        {% endif %}
                        if (error) {
                            {% if type.is_builder %}
                                self->HandleError("Bad value in {{suffix}}");
                            {% else %}
                                self->GetDevice()->HandleError("Bad value in {{suffix}}");
                            {% endif %}
                            return false;
                        }
                    {% endfor %}
                    return true;
                }

                //* Entry point with validation
                {{as_backendType(method.return_type)}} Validating{{suffix}}(
                    {{-as_backendType(type)}} self
                    {%- for arg in method.arguments -%}
                        , {{as_annotated_backendType(arg)}}
                    {%- endfor -%}
                ) {
                    //* Do the autogenerated checks
                    bool valid = ValidateBase{{suffix}}(self
                        {%- for arg in method.arguments -%}
                            , {{as_varName(arg.name)}}
                        {%- endfor -%}
                    );

                    //* Some function have very heavy checks in a separate method, so that they
                    //* can be skipped in the NonValidatingEntryPoints.
                    {% if suffix in methodsWithExtraValidation %}
                        if (valid) {
                            MaybeError error = self->Validate{{method.name.CamelCase()}}(
                                {%- for arg in method.arguments -%}
                                    {% if not loop.first %}, {% endif %}{{as_varName(arg.name)}}
                                {%- endfor -%}
                            );
                            //* Builders want to handle error themselves, unpack the error and make
                            //* the builder handle it.
                            {% if type.is_builder %}
                                if (error.IsError()) {
                                    ErrorData* errorData = error.AcquireError();
                                    self->HandleError(errorData->GetMessage().c_str());
                                    delete errorData;
                                    valid = false;
                                }
                            {% else %}
                                //* Non-builder errors are handled by the device
                                valid = !self->GetDevice()->ConsumedError(std::move(error));
                            {% endif %}
                        }
                    {% endif %}

                    //* HACK(cwallez@chromium.org): special casing GetResult so that the error callback
                    //* is called if needed. Without this, no call to HandleResult would happen, and the
                    //* error callback would always get called with an Unknown status
                    {% if type.is_builder and method.name.canonical_case() == "get result" %}
                        if (!valid) {
                            {{as_backendType(method.return_type)}} fakeResult = nullptr;
                            bool shouldBeFalse = self->HandleResult(fakeResult);
                            ASSERT(shouldBeFalse == false);
                        }
                    {% endif %}

                    {% if method.return_type.name.canonical_case() == "void" %}
                        if (!valid) return;
                    {% else %}
                        if (!valid) {
                            return {};
                        }
                        auto result =
                    {%- endif %}
                    self->{{method.name.CamelCase()}}(
                        {%- for arg in method.arguments -%}
                            {%- if not loop.first %}, {% endif -%}
                            {%- if arg.type.category in ["enum", "bitmask"] -%}
                                static_cast<dawn::{{as_cppType(arg.type.name)}}>({{as_varName(arg.name)}})
                            {%- elif arg.type.category == "structure" and arg.annotation != "value" -%}
                                reinterpret_cast<const dawn::{{as_cppType(arg.type.name)}}*>({{as_varName(arg.name)}})
                            {%- else -%}
                                {{as_varName(arg.name)}}
                            {%- endif -%}
                        {%- endfor -%}
                    );
                    {% if method.return_type.name.canonical_case() != "void" %}
                        return reinterpret_cast<{{as_backendType(method.return_type)}}>(result);
                    {% endif %}
                }
            {% endfor %}
        {% endfor %}
    }

    dawnProcTable GetNonValidatingProcs() {
        dawnProcTable table;
        {% for type in by_category["object"] %}
            {% for method in native_methods(type) %}
                table.{{as_varName(type.name, method.name)}} = reinterpret_cast<{{as_cProc(type.name, method.name)}}>(NonValidating{{as_MethodSuffix(type.name, method.name)}});
            {% endfor %}
        {% endfor %}
        return table;
    }

    dawnProcTable GetValidatingProcs() {
        dawnProcTable table;
        {% for type in by_category["object"] %}
            {% for method in native_methods(type) %}
                table.{{as_varName(type.name, method.name)}} = reinterpret_cast<{{as_cProc(type.name, method.name)}}>(Validating{{as_MethodSuffix(type.name, method.name)}});
            {% endfor %}
        {% endfor %}
        return table;
    }
}
}
