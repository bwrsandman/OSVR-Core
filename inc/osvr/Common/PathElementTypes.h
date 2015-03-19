/** @file
    @brief Header

    @date 2014

    Closely related to:

    - <osvr/Common/PathElementTypes_fwd.h>
    - <osvr/Common/PathElementTools.h>
    - <src/osvr/Common/PathElementTools.cpp>
    - <src/osvr/Common/PathTreeSerialization.cpp>

    Changes in this file may require changes in those other files.

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

// Copyright 2014 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef INCLUDED_PathElementTypes_h_GUID_5CC817E5_C7CB_45AE_399D_0B0D39579374
#define INCLUDED_PathElementTypes_h_GUID_5CC817E5_C7CB_45AE_399D_0B0D39579374

// Internal Includes
#include <osvr/Common/Export.h>
#include <osvr/Common/PathElementTypes_fwd.h> // IWYU pragma: export

// Library/third-party includes
#include <boost/variant/variant.hpp>
#include <boost/mpl/contains.hpp>

// Standard includes
#include <string>
#include <type_traits>

namespace osvr {
namespace common {
/// @brief Namespace for the various element types that may constitute a
/// node in the path tree.
#ifndef OSVR_DOXYGEN_EXTERNAL
///
/// Note that any changes that add/remove data members should result in
/// corresponding serialization changes in
/// src/osvr/Common/PathTreeSerialization.cpp
#endif
    namespace elements {
        /// @brief Base, using the CRTP, providing some basic functionality for
        /// path elements.
        template <typename Type> class ElementBase {
          public:
            typedef Type type;
            typedef ElementBase<Type> base_type;

            const char *getTypeName() const;

          protected:
            /// @brief Protected constructor to force subclassing.
            ///
            /// Inline implementation at the bottom of the file contains static
            /// assertions.
            ElementBase();
        };

        /// @brief The element type created when requesting a path that isn't
        /// yet in the tree.
        class NullElement : public ElementBase<NullElement> {
          public:
            NullElement() = default;
        };

        /// @brief The element type corresponding to a plugin
        class PluginElement : public ElementBase<PluginElement> {
          public:
            PluginElement() = default;
        };

        /// @brief The element type corresponding to a device, which implements
        /// 0 or more interfaces
        class DeviceElement : public ElementBase<DeviceElement> {
          public:
            DeviceElement() = default;
            DeviceElement(std::string const &deviceName,
                          std::string const &server)
                : m_devName(deviceName), m_server(server) {}

            OSVR_COMMON_EXPORT static DeviceElement
            createVRPNDeviceElement(std::string const &deviceName,
                                    std::string const &server);

            OSVR_COMMON_EXPORT std::string &getDeviceName();
            OSVR_COMMON_EXPORT std::string const &getDeviceName() const;
            OSVR_COMMON_EXPORT std::string &getServer();
            OSVR_COMMON_EXPORT std::string const &getServer() const;
            OSVR_COMMON_EXPORT std::string getFullDeviceName() const;

          private:
            std::string m_devName;
            std::string m_server;
        };

        /// @brief The element type corresponding to an interface, which often
        /// may have one or more sensors
        class InterfaceElement : public ElementBase<InterfaceElement> {
          public:
            InterfaceElement() = default;
        };

        /// @brief The element type corresponding to a particular sensor of an
        /// interface
        class SensorElement : public ElementBase<SensorElement> {
          public:
            SensorElement() = default;
        };

        /// @brief The element type serving as a physical association of other
        /// elements.
        class PhysicalAssociationElement
            : public ElementBase<PhysicalAssociationElement> {};

        /// @brief The element type corresponding to a logical grouping.
        class LogicalElement : public ElementBase<LogicalElement> {};

        /// @brief The element type corresponding to a "shallow" path alias -
        /// does not alias children.
        class AliasElement : public ElementBase<AliasElement> {
          public:
            /// @brief Constructor with source.
            OSVR_COMMON_EXPORT AliasElement(std::string const &source);

            /// @brief default constructor
            AliasElement() : m_automatic(false){};

            /// @brief Sets the source of this alias
            /// @param source absolute path of the target, possibly wrapped in
            /// transforms.
            /// @todo support relative paths - either here or at a different
            /// level
            void setSource(std::string const &source);

            /// @brief Get the source of data for this alias
            OSVR_COMMON_EXPORT std::string &getSource();
            /// @overload
            OSVR_COMMON_EXPORT std::string const &getSource() const;

            /// @brief Get/set whether this alias was automatically set (and
            /// thus subject to being override by explicit routing)
            OSVR_COMMON_EXPORT bool &getAutomatic();
            /// @overload
            OSVR_COMMON_EXPORT bool getAutomatic() const;

          private:
            std::string m_source;
            bool m_automatic;
        };

        /// This inline implementation MUST remain at the bottom of this file,
        /// after all full declarations of types to be included in PathElement.
        /// It consists entirely of compile time checks, so it is effectively
        /// removed from the code once the conditions are verified.
        template <typename Type> inline ElementBase<Type>::ElementBase() {
            /// Partially enforce the Curiously-Recurring Template Pattern.
            /// The assertion here is that for some `ElementBase<X>`, there
            /// exists a `class X : public ElementBase<X> {};`
            /// Doesn't prevent inheriting from the wrong base (`class X :
            /// public ElementBase<Y> {};` where there is already a `class Y :
            /// public ElementBase<Y> {};` - we have a static assert in the .cpp
            /// file to handle that for the types in the PathElement type list.
            static_assert(std::is_base_of<base_type, type>::value,
                          "ElementBase<T> must be the base of an element "
                          "type T (the CRTP)!");

            /// Enforce that every element type (that gets instantiated) has to
            /// be holdable by the PathElement variant
            static_assert(
                boost::mpl::contains<PathElement::types, type>::type::value,
                "Every element type must be a part of the PathElement variant "
                "type's bounded type list!");
        }

    } // namespace elements

} // namespace common
} // namespace osvr

#endif // INCLUDED_PathElementTypes_h_GUID_5CC817E5_C7CB_45AE_399D_0B0D39579374
