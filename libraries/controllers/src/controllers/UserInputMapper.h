//
//  Created by Sam Gateau on 4/27/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once
#ifndef hifi_UserInputMapper_h
#define hifi_UserInputMapper_h

#include <glm/glm.hpp>

#include <unordered_set>
#include <functional>
#include <memory>
#include <mutex>

#include <QtQml/QJSValue>
#include <QtScript/QScriptValue>

#include <DependencyManager.h>
#include <RegisteredMetaTypes.h>

#include "Forward.h"
#include "Pose.h"
#include "Input.h"
#include "InputDevice.h"
#include "DeviceProxy.h"
#include "StandardControls.h"
#include "Actions.h"

namespace controller {

    class RouteBuilderProxy;
    class MappingBuilderProxy;

    class UserInputMapper : public QObject, public Dependency {
        Q_OBJECT
        SINGLETON_DEPENDENCY
        Q_ENUMS(Action)

    public:
        // FIXME move to unordered set / map
        using EndpointToInputMap = std::map<EndpointPointer, Input>;
        using MappingNameMap = std::map<QString, MappingPointer>;
        using MappingDeviceMap = std::map<uint16_t, MappingPointer>;
        using MappingStack = std::list<MappingPointer>;
        using InputToEndpointMap = std::map<Input, EndpointPointer>;
        using EndpointSet = std::unordered_set<EndpointPointer>;
        using EndpointPair = std::pair<EndpointPointer, EndpointPointer>;
        using EndpointPairMap = std::map<EndpointPair, EndpointPointer>;
        using DevicesMap = std::map<int, InputDevice::Pointer>;
        using uint16 = uint16_t;
        using uint32 = uint32_t;

        static const uint16_t ACTIONS_DEVICE;
        static const uint16_t STANDARD_DEVICE;

        UserInputMapper();
        virtual ~UserInputMapper();


        static void registerControllerTypes(QScriptEngine* engine);

        void registerDevice(InputDevice::Pointer device);
        InputDevice::Pointer getDevice(const Input& input);
        QString getDeviceName(uint16 deviceID);

        Input::NamedVector getAvailableInputs(uint16 deviceID) const;
        Input::NamedVector getActionInputs() const { return getAvailableInputs(ACTIONS_DEVICE); }
        Input::NamedVector getStandardInputs() const { return getAvailableInputs(STANDARD_DEVICE); }

        int findDevice(QString name) const;
        QVector<QString> getDeviceNames();
        Input findDeviceInput(const QString& inputName) const;

        QVector<Action> getAllActions() const;
        QString getActionName(Action action) const;
        float getActionState(Action action) const { return _actionStates[toInt(action)]; }
        Pose getPoseState(Action action) const { return _poseStates[toInt(action)]; }
        int findAction(const QString& actionName) const;
        QVector<QString> getActionNames() const;
        Input inputFromAction(Action action) const { return getActionInputs()[toInt(action)].first; }

        void setActionState(Action action, float value) { _actionStates[toInt(action)] = value; }
        void deltaActionState(Action action, float delta) { _actionStates[toInt(action)] += delta; }
        void setActionState(Action action, const Pose& value) { _poseStates[toInt(action)] = value; }

        static Input makeStandardInput(controller::StandardButtonChannel button);
        static Input makeStandardInput(controller::StandardAxisChannel axis);
        static Input makeStandardInput(controller::StandardPoseChannel pose);

        void removeDevice(int device);

        // Update means go grab all the device input channels and update the output channel values
        void update(float deltaTime);

        void setSensorToWorldMat(glm::mat4 sensorToWorldMat) { _sensorToWorldMat = sensorToWorldMat; }
        glm::mat4 getSensorToWorldMat() { return _sensorToWorldMat; }

        DevicesMap getDevices() { return _registeredDevices; }
        uint16 getStandardDeviceID() const { return STANDARD_DEVICE; }
        InputDevice::Pointer getStandardDevice() { return _registeredDevices[getStandardDeviceID()]; }

        MappingPointer newMapping(const QString& mappingName);
        MappingPointer parseMapping(const QString& json);
        MappingPointer loadMapping(const QString& jsonFile);

        void loadDefaultMapping(uint16 deviceID);
        void enableMapping(const QString& mappingName, bool enable = true);
        float getValue(const Input& input) const;
        Pose getPose(const Input& input) const;

    signals:
        void actionEvent(int action, float state);
        void inputEvent(int input, float state);
        void hardwareChanged();

    protected:
        // GetFreeDeviceID should be called before registering a device to use an ID not used by a different device.
        uint16 getFreeDeviceID() { return _nextFreeDeviceID++; }
        DevicesMap _registeredDevices;
        uint16 _nextFreeDeviceID = STANDARD_DEVICE + 1;

        std::vector<float> _actionStates = std::vector<float>(toInt(Action::NUM_ACTIONS), 0.0f);
        std::vector<float> _actionScales = std::vector<float>(toInt(Action::NUM_ACTIONS), 1.0f);
        std::vector<float> _lastActionStates = std::vector<float>(toInt(Action::NUM_ACTIONS), 0.0f);
        std::vector<Pose> _poseStates = std::vector<Pose>(toInt(Action::NUM_ACTIONS));
        std::vector<float> _lastStandardStates = std::vector<float>();

        glm::mat4 _sensorToWorldMat;

        int recordDeviceOfType(const QString& deviceName);
        QHash<const QString&, int> _deviceCounts;

        static float getValue(const EndpointPointer& endpoint, bool peek = false);
        static Pose getPose(const EndpointPointer& endpoint, bool peek = false);

        friend class RouteBuilderProxy;
        friend class MappingBuilderProxy;

        void runMappings();

        static void applyRoutes(const RouteList& route);
        static bool applyRoute(const RoutePointer& route, bool force = false);
        void enableMapping(const MappingPointer& mapping);
        void disableMapping(const MappingPointer& mapping);
        EndpointPointer endpointFor(const QJSValue& endpoint);
        EndpointPointer endpointFor(const QScriptValue& endpoint);
        EndpointPointer endpointFor(const Input& endpoint) const;
        EndpointPointer compositeEndpointFor(EndpointPointer first, EndpointPointer second);
        ConditionalPointer conditionalFor(const QJSValue& endpoint);
        ConditionalPointer conditionalFor(const QScriptValue& endpoint);
        ConditionalPointer conditionalFor(const Input& endpoint) const;
        
        MappingPointer parseMapping(const QJsonValue& json);
        RoutePointer parseRoute(const QJsonValue& value);
        EndpointPointer parseDestination(const QJsonValue& value);
        EndpointPointer parseSource(const QJsonValue& value);
        EndpointPointer parseAxis(const QJsonValue& value);
        EndpointPointer parseAny(const QJsonValue& value);
        EndpointPointer parseEndpoint(const QJsonValue& value);
        ConditionalPointer parseConditional(const QJsonValue& value);

        static FilterPointer parseFilter(const QJsonValue& value);
        static FilterList parseFilters(const QJsonValue& value);

        InputToEndpointMap _endpointsByInput;
        EndpointToInputMap _inputsByEndpoint;
        EndpointPairMap _compositeEndpoints;

        MappingNameMap _mappingsByName;
        MappingDeviceMap _mappingsByDevice;

        RouteList _deviceRoutes;
        RouteList _standardRoutes;

        using Locker = std::unique_lock<std::recursive_mutex>;

        mutable std::recursive_mutex _lock;
    };

}

Q_DECLARE_METATYPE(controller::Input::NamedPair)
Q_DECLARE_METATYPE(controller::Pose)
Q_DECLARE_METATYPE(QVector<controller::Input::NamedPair>)
Q_DECLARE_METATYPE(controller::Input)
Q_DECLARE_METATYPE(controller::Action)
Q_DECLARE_METATYPE(QVector<controller::Action>)

// Cheating.
using UserInputMapper = controller::UserInputMapper;

#endif // hifi_UserInputMapper_h
