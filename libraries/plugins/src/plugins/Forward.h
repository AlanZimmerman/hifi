//
//  Created by Bradley Austin Davis on 2015/08/08
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#pragma once

#include <QList>
#include <QVector>
#include <QSharedPointer>

class DisplayPlugin;
class InputPlugin;
class Plugin;
class PluginContainer;
class PluginManager;

using DisplayPluginPointer = QSharedPointer<DisplayPlugin>;
using DisplayPluginList = QVector<DisplayPluginPointer>;
using InputPluginPointer = QSharedPointer<InputPlugin>;
using InputPluginList = QVector<InputPluginPointer>;

