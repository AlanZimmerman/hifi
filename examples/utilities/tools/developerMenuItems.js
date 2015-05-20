//
//  developerMenuItems.js
//  examples
//
//  Created by Brad Hefta-Gaub on 2/24/14
//  Copyright 2013 High Fidelity, Inc.
//
//  Adds a bunch of developer and debugging menu items
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

var createdRenderMenu = false;
var createdGeneratedAudioMenu = false;
var createdStereoInputMenuItem = false;

var DEVELOPER_MENU = "Developer";

var ENTITIES_MENU = DEVELOPER_MENU + " > Entities";
var COLLISION_UPDATES_TO_SERVER = "Don't send collision updates to server";

var RENDER_MENU = DEVELOPER_MENU + " > Render";
var ENTITIES_ITEM = "Entities";
var AVATARS_ITEM = "Avatars";

var AUDIO_MENU = DEVELOPER_MENU + " > Audio";
var AUDIO_SOURCE_INJECT = "Generated Audio";
var AUDIO_SOURCE_MENU = AUDIO_MENU + " > Generated Audio Source";
var AUDIO_SOURCE_PINK_NOISE = "Pink Noise";
var AUDIO_SOURCE_SINE_440 = "Sine 440hz";
var AUDIO_STEREO_INPUT = "Stereo Input";


function setupMenus() {
    if (!Menu.menuExists(DEVELOPER_MENU)) {
        Menu.addMenu(DEVELOPER_MENU);
    }
    if (!Menu.menuExists(ENTITIES_MENU)) {
        Menu.addMenu(ENTITIES_MENU);
        
        // NOTE: these menu items aren't currently working. I've temporarily removed them. Will add them back once we
        // rewire these to work
        /*
        Menu.addMenuItem({ menuName: "Developer > Entities", menuItemName: "Display Model Bounds", isCheckable: true, isChecked: false });
        Menu.addMenuItem({ menuName: "Developer > Entities", menuItemName: "Display Model Triangles", isCheckable: true, isChecked: false });
        Menu.addMenuItem({ menuName: "Developer > Entities", menuItemName: "Display Model Element Bounds", isCheckable: true, isChecked: false });
        Menu.addMenuItem({ menuName: "Developer > Entities", menuItemName: "Display Model Element Children", isCheckable: true, isChecked: false });
        Menu.addMenuItem({ menuName: "Developer > Entities", menuItemName: "Don't Do Precision Picking", isCheckable: true, isChecked: false });
        Menu.addMenuItem({ menuName: "Developer > Entities", menuItemName: "Don't Attempt Render Entities as Scene", isCheckable: true, isChecked: false });
        Menu.addMenuItem({ menuName: "Developer > Entities", menuItemName: "Don't Do Precision Picking", isCheckable: true, isChecked: false });
        Menu.addMenuItem({ menuName: "Developer > Entities", menuItemName: "Disable Light Entities", isCheckable: true, isChecked: false });
        */
        Menu.addMenuItem({ menuName: ENTITIES_MENU, menuItemName: COLLISION_UPDATES_TO_SERVER, isCheckable: true, isChecked: false });
    }
    
    if (!Menu.menuExists(RENDER_MENU)) {
        Menu.addMenu(RENDER_MENU);
        createdRenderMenu = true;
    }
    
    if (!Menu.menuItemExists(RENDER_MENU, ENTITIES_ITEM)) {
        Menu.addMenuItem({ menuName: RENDER_MENU, menuItemName: ENTITIES_ITEM, isCheckable: true, isChecked: Scene.shouldRenderEntities })
    }
    
    if (!Menu.menuItemExists(RENDER_MENU, AVATARS_ITEM)) {
        Menu.addMenuItem({ menuName: RENDER_MENU, menuItemName: AVATARS_ITEM, isCheckable: true, isChecked: Scene.shouldRenderAvatars })
    }
    
    
    if (!Menu.menuExists(AUDIO_MENU)) {
        Menu.addMenu(AUDIO_MENU);
    }
    if (!Menu.menuItemExists(AUDIO_MENU, AUDIO_SOURCE_INJECT)) {
        Menu.addMenuItem({ menuName: AUDIO_MENU, menuItemName: AUDIO_SOURCE_INJECT, isCheckable: true, isChecked: false });
        Menu.addMenu(AUDIO_SOURCE_MENU);
        Menu.addMenuItem({ menuName: AUDIO_SOURCE_MENU, menuItemName: AUDIO_SOURCE_PINK_NOISE, isCheckable: true, isChecked: false });
        Menu.addMenuItem({ menuName: AUDIO_SOURCE_MENU, menuItemName: AUDIO_SOURCE_SINE_440, isCheckable: true, isChecked: false });
        Menu.setIsOptionChecked(AUDIO_SOURCE_PINK_NOISE, true);
        Audio.selectPinkNoise();
        createdGeneratedAudioMenu = true;
    }
    if (!Menu.menuItemExists(AUDIO_MENU, AUDIO_STEREO_INPUT)) {
        Menu.addMenuItem({ menuName: AUDIO_MENU, menuItemName: AUDIO_STEREO_INPUT, isCheckable: true, isChecked: false });
        createdStereoInputMenuItem = true;
    }
}

Menu.menuItemEvent.connect(function (menuItem) {
    print("menuItemEvent() in JS... menuItem=" + menuItem);

    if (menuItem == COLLISION_UPDATES_TO_SERVER) {
        var dontSendUpdates = Menu.isOptionChecked(COLLISION_UPDATES_TO_SERVER); 
        print("  dontSendUpdates... checked=" + dontSendUpdates);
        Entities.setSendPhysicsUpdates(!dontSendUpdates);
    } else if (menuItem == ENTITIES_ITEM) {
        Scene.shouldRenderEntities = Menu.isOptionChecked(ENTITIES_ITEM);
    } else if (menuItem == AVATARS_ITEM) {
        Scene.shouldRenderAvatars = Menu.isOptionChecked(AVATARS_ITEM);
    } else if (menuItem == AUDIO_SOURCE_INJECT && !createdGeneratedAudioMenu) {
        Audio.injectGeneratedNoise(Menu.isOptionChecked(AUDIO_SOURCE_INJECT));
   } else if (menuItem == AUDIO_SOURCE_PINK_NOISE && !createdGeneratedAudioMenu) {
       Audio.selectPinkNoise();
       Menu.setIsOptionChecked(AUDIO_SOURCE_SINE_440, false);
   } else if (menuItem == AUDIO_SOURCE_SINE_440 && !createdGeneratedAudioMenu) {
       Audio.selectSine440();
       Menu.setIsOptionChecked(AUDIO_SOURCE_PINK_NOISE, false);
   } else if (menuItem == AUDIO_STEREO_INPUT) {
       Audio.setStereoInput(Menu.isOptionChecked(AUDIO_STEREO_INPUT))
   }
});

Scene.shouldRenderAvatarsChanged.connect(function(shouldRenderAvatars) {
    Menu.setIsOptionChecked(AVATARS_ITEM, shouldRenderAvatars)
});

Scene.shouldRenderEntitiesChanged.connect(function(shouldRenderEntities) {
    Menu.setIsOptionChecked(ENTITIES_ITEM, shouldRenderEntities)
});

function scriptEnding() {
    Menu.removeMenu(ENTITIES_MENU);
    
    if (createdRenderMenu) {
        Menu.removeMenu(RENDER_MENU);
    } else {
        Menu.removeMenuItem(RENDER_MENU, ENTITIES_ITEM);
        Menu.removeMenuItem(RENDER_MENU, AVATARS_ITEM);
    }
    
    if (createdGeneratedAudioMenu) {
        Audio.injectGeneratedNoise(false);
        Menu.removeMenuItem(AUDIO_MENU, AUDIO_SOURCE_INJECT);
        Menu.removeMenu(AUDIO_SOURCE_MENU);
    }

    if (createdStereoInputMenuItem) {
        Menu.removeMenuItem(AUDIO_MENU, AUDIO_STEREO_INPUT);
    }
}

setupMenus();
Script.scriptEnding.connect(scriptEnding);
