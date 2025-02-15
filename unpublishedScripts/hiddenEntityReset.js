//
//  hiddenEntityReset.js
//
//  Created by Eric Levin on 10/2/15.
//  Copyright 2015 High Fidelity, Inc.
//
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//


(function() {

    var _this;

    var sprayPaintScriptURL = Script.resolvePath("../examples/toybox/spray_paint/sprayPaintCan.js");
    var catScriptURL = Script.resolvePath("../examples/toybox/cat/cat.js");
    var flashlightScriptURL = Script.resolvePath('../examples/toybox/flashlight/flashlight.js');
    var pingPongScriptURL = Script.resolvePath('../examples/toybox/ping_pong_gun/pingPongGun.js');
    var wandScriptURL = Script.resolvePath("../examples/toybox/bubblewand/wand.js");
    var dollScriptURL = Script.resolvePath("../examples/toybox/doll/doll.js");
    var lightsScriptURL = Script.resolvePath("../examples/toybox/lights/lightSwitch.js");
    var targetsScriptURL = Script.resolvePath('../examples/toybox/ping_pong_gun/wallTarget.js');
    var basketballResetterScriptURL = Script.resolvePath('basketballsResetter.js');
    var targetsResetterScriptURL = Script.resolvePath('targetsResetter.js');

    ResetSwitch = function() {
        _this = this;
    };

    ResetSwitch.prototype = {

        clickReleaseOnEntity: function(entityId, mouseEvent) {
            if (!mouseEvent.isLeftButton) {
                return;
            }
            this.triggerReset();

        },

        startNearGrabNonColliding: function() {
            this.triggerReset();
        },

        triggerReset: function() {
            MasterReset();
        },

        preload: function(entityID) {
            this.entityID = entityID;
        }

    };


    MasterReset = function() {
        var resetKey = "resetMe";

        var HIFI_PUBLIC_BUCKET = "http://s3.amazonaws.com/hifi-public/";

        var shouldDeleteOnEndScript = false;

        //Before creating anything, first search a radius and delete all the things that should be deleted
        deleteAllToys();
        createAllToys();

        function createAllToys() {
            createBlocks({
                x: 548.3,
                y: 495.55,
                z: 504.4
            });

            createBasketBall({
                x: 547.73,
                y: 495.5,
                z: 505.47
            });

            createDoll({
                x: 546.67,
                y: 495.41,
                z: 505.09
            });

            createWand({
                x: 546.71,
                y: 495.55,
                z: 506.15
            });

            createDice();

            createFlashlight({
                x: 545.72,
                y: 495.41,
                z: 505.78
            });

            createCombinedArmChair({
                x: 549.29,
                y: 494.9,
                z: 508.22
            });

            createPottedPlant({
                x: 554.26,
                y: 495.2,
                z: 504.53
            });

            createPingPongBallGun();
            createTargets();
            createTargetResetter();

            createBasketballRack();
            createBasketballResetter();

            createGates();

            createFire();
            // Handles toggling of all sconce lights 
            createLights();

            createCat({
                x: 551.09,
                y: 494.98,
                z: 503.49
            });

            createSprayCan({
                x: 549.7,
                y: 495.6,
                z: 503.91
            });
        }

        function deleteAllToys() {
            var entities = Entities.findEntities(MyAvatar.position, 100);

            entities.forEach(function(entity) {
                //params: customKey, id, defaultValue
                var shouldReset = getEntityCustomData(resetKey, entity, {}).resetMe;
                if (shouldReset === true) {
                    Entities.deleteEntity(entity);
                }
            });
        }

        function createFire() {


            var myOrientation = Quat.fromPitchYawRollDegrees(-90, 0, 0.0);

            var animationSettings = JSON.stringify({
                fps: 30,
                running: true,
                loop: true,
                firstFrame: 1,
                lastFrame: 10000
            });


            var fire = Entities.addEntity({
                type: "ParticleEffect",
                name: "fire",
                animationSettings: animationSettings,
                textures: "https://hifi-public.s3.amazonaws.com/alan/Particles/Particle-Sprite-Smoke-1.png",
                position: {
                    x: 551.45,
                    y: 494.82,
                    z: 502.05
                },
                emitRate: 100,
                colorStart: {
                    red: 70,
                    green: 70,
                    blue: 137
                },
                color: {
                    red: 200,
                    green: 99,
                    blue: 42
                },
                colorFinish: {
                    red: 255,
                    green: 99,
                    blue: 32
                },
                radiusSpread: 0.01,
                radiusStart: 0.02,
                radiusEnd: 0.001,
                particleRadius: 0.05,
                radiusFinish: 0.0,
                emitOrientation: myOrientation,
                emitSpeed: 0.3,
                speedSpread: 0.1,
                alphaStart: 0.05,
                alpha: 0.1,
                alphaFinish: 0.05,
                emitDimensions: {
                    x: 1,
                    y: 1,
                    z: 0.1
                },
                polarFinish: 0.1,
                emitAcceleration: {
                    x: 0.0,
                    y: 0.0,
                    z: 0.0
                },
                accelerationSpread: {
                    x: 0.1,
                    y: 0.01,
                    z: 0.1
                },
                lifespan: 1,
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true
                    }
                })
            });
        }

        function createBasketballRack() {
            var NUMBER_OF_BALLS = 4;
            var DIAMETER = 0.30;
            var RESET_DISTANCE = 1;
            var MINIMUM_MOVE_LENGTH = 0.05;
            var basketballURL = HIFI_PUBLIC_BUCKET + "models/content/basketball2.fbx";
            var basketballCollisionSoundURL = HIFI_PUBLIC_BUCKET + "sounds/basketball/basketball.wav";
            var rackURL = HIFI_PUBLIC_BUCKET + "models/basketball_hoop/basketball_rack.fbx";
            var rackCollisionHullURL = HIFI_PUBLIC_BUCKET + "models/basketball_hoop/rack_collision_hull.obj";

            var rackRotation = Quat.fromPitchYawRollDegrees(0, -90, 0);

            var rackStartPosition = {
                x: 542.86,
                y: 494.84,
                z: 475.06
            };
            var rack = Entities.addEntity({
                name: 'Basketball Rack',
                type: "Model",
                modelURL: rackURL,
                position: rackStartPosition,
                rotation: rackRotation,
                shapeType: 'compound',
                gravity: {
                    x: 0,
                    y: -9.8,
                    z: 0
                },
                linearDamping: 1,
                dimensions: {
                    x: 0.4,
                    y: 1.37,
                    z: 1.73
                },
                collisionsWillMove: true,
                ignoreForCollisions: false,
                compoundShapeURL: rackCollisionHullURL,
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true
                    },
                    grabbableKey: {
                        grabbable: false
                    }
                })
            });



            function createCollidingBalls() {
                var position = rackStartPosition;
                var collidingBalls = [];

                var i;
                for (i = 0; i < NUMBER_OF_BALLS; i++) {
                    var ballPosition = {
                        x: position.x,
                        y: position.y + DIAMETER * 2,
                        z: position.z + (DIAMETER) - (DIAMETER * i)
                    };
                    var newPosition = {
                        x: position.x + (DIAMETER * 2) - (DIAMETER * i),
                        y: position.y + DIAMETER * 2,
                        z: position.z
                    };
                    var collidingBall = Entities.addEntity({
                        type: "Model",
                        name: 'Hifi-Basketball',
                        shapeType: 'Sphere',
                        position: newPosition,
                        dimensions: {
                            x: DIAMETER,
                            y: DIAMETER,
                            z: DIAMETER
                        },
                        restitution: 1.0,
                        linearDamping: 0.00001,
                        gravity: {
                            x: 0,
                            y: -9.8,
                            z: 0
                        },
                        collisionsWillMove: true,
                        ignoreForCollisions: false,
                        modelURL: basketballURL,
                        userData: JSON.stringify({
                            originalPositionKey: {
                                originalPosition: newPosition
                            },
                            resetMe: {
                                resetMe: true
                            },
                            grabbableKey: {
                                invertSolidWhileHeld: true
                            }
                        })
                    });

                    collidingBalls.push(collidingBall);

                }
            }

            createCollidingBalls();

        }

        function createBasketballResetter() {

            var position = {
                x: 543.58,
                y: 495.47,
                z: 469.59
            };

            var dimensions = {
                x: 1.65,
                y: 1.71,
                z: 1.75
            };

            var resetter = Entities.addEntity({
                type: "Box",
                position: position,
                name: "Basketball Resetter",
                script: basketballResetterScriptURL,
                dimensions: dimensions,
                visible: false,
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true
                    },
                    grabbableKey: {
                        wantsTrigger: true
                    }
                })
            });


        }

        function createTargetResetter() {
            var dimensions = {
                x: 0.21,
                y: 0.61,
                z: 0.21
            };

            var position = {
                x: 548.42,
                y: 496.40,
                z: 509.61
            };

            var resetter = Entities.addEntity({
                type: "Box",
                position: position,
                name: "Target Resetter",
                script: targetsResetterScriptURL,
                dimensions: dimensions,
                visible: false,
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true
                    },
                    grabbableKey: {
                        wantsTrigger: true
                    }
                })

            });
        }



        function createTargets() {

            var MODEL_URL = 'http://hifi-public.s3.amazonaws.com/models/ping_pong_gun/target.fbx';
            var COLLISION_HULL_URL = 'http://hifi-public.s3.amazonaws.com/models/ping_pong_gun/target_collision_hull.obj';

            var MINIMUM_MOVE_LENGTH = 0.05;
            var RESET_DISTANCE = 0.5;
            var TARGET_USER_DATA_KEY = 'hifi-ping_pong_target';
            var NUMBER_OF_TARGETS = 6;
            var TARGETS_PER_ROW = 3;

            var TARGET_DIMENSIONS = {
                x: 0.06,
                y: 0.42,
                z: 0.42
            };

            var VERTICAL_SPACING = TARGET_DIMENSIONS.y + 0.5;
            var HORIZONTAL_SPACING = TARGET_DIMENSIONS.z + 0.5;


            var startPosition = {
                x: 548.68,
                y: 497.30,
                z: 509.74
            };

            var rotation = Quat.fromPitchYawRollDegrees(0, -55.25, 0);

            var targets = [];

            function addTargets() {
                var i;
                var row = -1;
                for (i = 0; i < NUMBER_OF_TARGETS; i++) {

                    if (i % TARGETS_PER_ROW === 0) {
                        row++;
                    }

                    var vHat = Quat.getFront(rotation);
                    var spacer = HORIZONTAL_SPACING * (i % TARGETS_PER_ROW) + (row * HORIZONTAL_SPACING / 2);
                    var multiplier = Vec3.multiply(spacer, vHat);
                    var position = Vec3.sum(startPosition, multiplier);
                    position.y = startPosition.y - (row * VERTICAL_SPACING);

                    var targetProperties = {
                        name: 'Hifi-Target',
                        type: 'Model',
                        modelURL: MODEL_URL,
                        shapeType: 'compound',
                        collisionsWillMove: true,
                        dimensions: TARGET_DIMENSIONS,
                        compoundShapeURL: COLLISION_HULL_URL,
                        position: position,
                        rotation: rotation,
                        script: targetsScriptURL,
                        userData: JSON.stringify({
                            originalPositionKey: {
                                originalPosition: position
                            },
                            resetMe: {
                                resetMe: true
                            },
                            grabbableKey: {
                                grabbable: false
                            }
                        })
                    };

                    var target = Entities.addEntity(targetProperties);
                    targets.push(target);

                }
            }

            addTargets();

        }

        function createCat(position) {

            var modelURL = "http://hifi-public.s3.amazonaws.com/ryan/Dark_Cat.fbx";
            var animationURL = "http://hifi-public.s3.amazonaws.com/ryan/sleeping.fbx";
            var animationSettings = JSON.stringify({
                running: true
            });
            var cat = Entities.addEntity({
                type: "Model",
                modelURL: modelURL,
                name: "cat",
                script: catScriptURL,
                animationURL: animationURL,
                animationSettings: animationSettings,
                position: position,
                rotation: {
                    w: 0.35020983219146729,
                    x: -4.57763671875e-05,
                    y: 0.93664455413818359,
                    z: -1.52587890625e-05
                },
                dimensions: {
                    x: 0.15723302960395813,
                    y: 0.50762706995010376,
                    z: 0.90716040134429932
                },
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true
                    },
                    grabbableKey: {
                        grabbable: false
                    }
                })
            });

        }

        function createFlashlight(position) {
            var modelURL = "https://hifi-public.s3.amazonaws.com/models/props/flashlight.fbx";

            var flashlight = Entities.addEntity({
                type: "Model",
                modelURL: modelURL,
                name: "flashlight",
                script: flashlightScriptURL,
                position: position,
                dimensions: {
                    x: 0.08,
                    y: 0.30,
                    z: 0.08
                },
                collisionsWillMove: true,
                gravity: {
                    x: 0,
                    y: -3.5,
                    z: 0
                },
                velocity: {
                    x: 0,
                    y: -0.01,
                    z: 0
                },
                shapeType: 'box',
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true
                    },
                    grabbableKey: {
                        invertSolidWhileHeld: true
                    }
                })
            });


        }

        function createLights() {
            var modelURL = "http://hifi-public.s3.amazonaws.com/ryan/lightswitch.fbx";


            var rotation = {
                w: 0.63280689716339111,
                x: 0.63280689716339111,
                y: -0.31551080942153931,
                z: 0.31548023223876953
            };
            var axis = {
                x: 0,
                y: 1,
                z: 0
            };
            var dQ = Quat.angleAxis(180, axis);
            rotation = Quat.multiply(rotation, dQ);

            var lightSwitchHall = Entities.addEntity({
                type: "Model",
                modelURL: modelURL,
                name: "Light Switch Hall",
                script: lightsScriptURL,
                position: {
                    x: 543.27764892578125,
                    y: 495.67999267578125,
                    z: 511.00564575195312
                },
                rotation: rotation,
                dimensions: {
                    x: 0.10546875,
                    y: 0.032372996211051941,
                    z: 0.16242524981498718
                },
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true,
                        on: true,
                        type: "Hall Light"
                    },
                    grabbableKey: {
                        wantsTrigger: true
                    }
                })
            });

            var sconceLight1 = Entities.addEntity({
                type: "Light",
                position: {
                    x: 543.75,
                    y: 496.24,
                    z: 511.13
                },
                name: "Sconce 1 Light",
                dimensions: {
                    x: 2.545,
                    y: 2.545,
                    z: 2.545
                },
                cutoff: 90,
                color: {
                    red: 217,
                    green: 146,
                    blue: 24
                },
                isSpotlight: false,
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true,
                        type: "Hall Light"
                    }
                })
            });

            var sconceLight2 = Entities.addEntity({
                type: "Light",
                position: {
                    x: 540.1,
                    y: 496.24,
                    z: 505.57
                },
                name: "Sconce 2 Light",
                dimensions: {
                    x: 2.545,
                    y: 2.545,
                    z: 2.545
                },
                cutoff: 90,
                color: {
                    red: 217,
                    green: 146,
                    blue: 24
                },
                isSpotlight: false,
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true,
                        type: "Hall Light"
                    }
                })
            });

            rotation = {
                w: 0.20082402229309082,
                x: 0.20082402229309082,
                y: -0.67800414562225342,
                z: 0.67797362804412842
            };
            axis = {
                x: 0,
                y: 1,
                z: 0
            };
            dQ = Quat.angleAxis(180, axis);
            rotation = Quat.multiply(rotation, dQ);

            var lightSwitchGarage = Entities.addEntity({
                type: "Model",
                modelURL: modelURL,
                name: "Light Switch Garage",
                script: lightsScriptURL,
                position: {
                    x: 545.62,
                    y: 495.68,
                    z: 500.21
                },
                rotation: rotation,
                dimensions: {
                    x: 0.10546875,
                    y: 0.032372996211051941,
                    z: 0.16242524981498718
                },
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true,
                        on: true,
                        type: "Garage Light"
                    },
                    grabbableKey: {
                        wantsTrigger: true
                    }
                })
            });



            var sconceLight3 = Entities.addEntity({
                type: "Light",
                position: {
                    x: 545.49468994140625,
                    y: 496.24026489257812,
                    z: 500.63516235351562
                },

                name: "Sconce 3 Light",
                dimensions: {
                    x: 2.545,
                    y: 2.545,
                    z: 2.545
                },
                cutoff: 90,
                color: {
                    red: 217,
                    green: 146,
                    blue: 24
                },
                isSpotlight: false,
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true,
                        type: "Garage Light"
                    }
                })
            });


            var sconceLight4 = Entities.addEntity({
                type: "Light",
                position: {
                    x: 550.90399169921875,
                    y: 496.24026489257812,
                    z: 507.90237426757812
                },
                name: "Sconce 4 Light",
                dimensions: {
                    x: 2.545,
                    y: 2.545,
                    z: 2.545
                },
                cutoff: 90,
                color: {
                    red: 217,
                    green: 146,
                    blue: 24
                },
                isSpotlight: false,
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true,
                        type: "Garage Light"
                    }
                })
            });

            var sconceLight5 = Entities.addEntity({
                type: "Light",
                position: {
                    x: 548.407958984375,
                    y: 496.24026489257812,
                    z: 509.5504150390625
                },
                name: "Sconce 5 Light",
                dimensions: {
                    x: 2.545,
                    y: 2.545,
                    z: 2.545
                },
                cutoff: 90,
                color: {
                    red: 217,
                    green: 146,
                    blue: 24
                },
                isSpotlight: false,
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true,
                        type: "Garage Light"
                    }
                })
            });

        }



        function createDice() {
            var diceProps = {
                type: "Model",
                modelURL: "http://s3.amazonaws.com/hifi-public/models/props/Dice/goldDie.fbx",
                collisionSoundURL: "http://s3.amazonaws.com/hifi-public/sounds/dice/diceCollide.wav",
                name: "dice",
                position: {
                    x: 541,
                    y: 494.96,
                    z: 509.1
                },
                dimensions: {
                    x: 0.09,
                    y: 0.09,
                    z: 0.09
                },
                gravity: {
                    x: 0,
                    y: -3.5,
                    z: 0
                },
                velocity: {
                    x: 0,
                    y: -0.01,
                    z: 0
                },
                shapeType: "box",
                collisionsWillMove: true,
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true
                    },
                    grabbableKey: {
                        invertSolidWhileHeld: true
                    }

                })
            };
            var dice1 = Entities.addEntity(diceProps);

            diceProps.position = {
                x: 541.05,
                y: 494.96,
                z: 509.0
            };

            var dice2 = Entities.addEntity(diceProps);

        }


        function createGates() {
            var MODEL_URL = 'http://hifi-public.s3.amazonaws.com/ryan/fence.fbx';

            var rotation = Quat.fromPitchYawRollDegrees(0, -16, 0);
            var gate = Entities.addEntity({
                name: 'Front Door Fence',
                type: 'Model',
                modelURL: MODEL_URL,
                shapeType: 'box',
                position: {
                    x: 531.15,
                    y: 495.11,
                    z: 520.20
                },
                dimensions: {
                    x: 1.42,
                    y: 1.13,
                    z: 0.2
                },
                rotation: rotation,
                collisionsWillMove: true,
                gravity: {
                    x: 0,
                    y: -100,
                    z: 0
                },
                linearDamping: 1,
                angularDamping: 10,
                mass: 10,
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true
                    },
                    grabbableKey: {
                        grabbable: false
                    }
                })
            });
        }

        function createPingPongBallGun() {
            var MODEL_URL = 'http://hifi-public.s3.amazonaws.com/models/ping_pong_gun/ping_pong_gun.fbx';
            var COLLISION_HULL_URL = 'http://hifi-public.s3.amazonaws.com/models/ping_pong_gun/ping_pong_gun_collision_hull.obj';

            var position = {
                x: 548.6,
                y: 495.4,
                z: 503.39
            };

            var rotation = Quat.fromPitchYawRollDegrees(0, 36, 0);

            var pingPongGun = Entities.addEntity({
                type: "Model",
                modelURL: MODEL_URL,
                shapeType: 'box',
                script: pingPongScriptURL,
                position: position,
                rotation: rotation,
                gravity: {
                    x: 0,
                    y: -9.8,
                    z: 0
                },
                dimensions: {
                    x: 0.08,
                    y: 0.21,
                    z: 0.47
                },
                collisionsWillMove: true,
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true
                    },
                    grabbableKey: {
                        invertSolidWhileHeld: true
                    }

                })
            });
        }

        function createWand(position) {
            var WAND_MODEL = 'http://hifi-public.s3.amazonaws.com/james/bubblewand/models/wand/wand.fbx';
            var WAND_COLLISION_SHAPE = 'http://hifi-public.s3.amazonaws.com/james/bubblewand/models/wand/actual_no_top_collision_hull.obj';

            var entity = Entities.addEntity({
                name: 'Bubble Wand',
                type: "Model",
                modelURL: WAND_MODEL,
                position: position,
                gravity: {
                    x: 0,
                    y: -9.8,
                    z: 0
                },
                dimensions: {
                    x: 0.05,
                    y: 0.25,
                    z: 0.05
                },
                //must be enabled to be grabbable in the physics engine
                shapeType: 'compound',
                collisionsWillMove: true,
                compoundShapeURL: WAND_COLLISION_SHAPE,
                //Look into why bubble wand is going through table when gravity is enabled
                // gravity: {x: 0, y: -3.5, z: 0},
                // velocity: {x: 0, y: -0.01, z:0},
                script: wandScriptURL,
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true
                    },
                    grabbableKey: {
                        invertSolidWhileHeld: true
                    }
                })
            });


        }

        function createBasketBall(position) {

            var modelURL = "http://s3.amazonaws.com/hifi-public/models/content/basketball2.fbx";

            var entity = Entities.addEntity({
                type: "Model",
                modelURL: modelURL,
                position: position,
                collisionsWillMove: true,
                shapeType: "sphere",
                name: "basketball",
                dimensions: {
                    x: 0.25,
                    y: 0.26,
                    z: 0.25
                },
                gravity: {
                    x: 0,
                    y: -7,
                    z: 0
                },
                restitution: 10,
                linearDamping: 0.0,
                velocity: {
                    x: 0,
                    y: -0.01,
                    z: 0
                },
                collisionSoundURL: "http://s3.amazonaws.com/hifi-public/sounds/basketball/basketball.wav",
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true
                    },
                    grabbableKey: {
                        invertSolidWhileHeld: true
                    }
                })
            });

        }

        function createDoll(position) {
            var modelURL = "http://hifi-public.s3.amazonaws.com/models/Bboys/bboy2/bboy2.fbx";

            var naturalDimensions = {
                x: 1.63,
                y: 1.67,
                z: 0.26
            };
            var desiredDimensions = Vec3.multiply(naturalDimensions, 0.15);
            var entity = Entities.addEntity({
                type: "Model",
                name: "doll",
                modelURL: modelURL,
                script: dollScriptURL,
                position: position,
                shapeType: 'box',
                dimensions: desiredDimensions,
                gravity: {
                    x: 0,
                    y: -5,
                    z: 0
                },
                velocity: {
                    x: 0,
                    y: -0.1,
                    z: 0
                },
                collisionsWillMove: true,
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true
                    },
                    grabbableKey: {
                        invertSolidWhileHeld: true
                    }
                })
            });

        }

        function createSprayCan(position) {

            var modelURL = "https://hifi-public.s3.amazonaws.com/eric/models/paintcan.fbx";

            var entity = Entities.addEntity({
                type: "Model",
                name: "spraycan",
                script: sprayPaintScriptURL,
                modelURL: modelURL,
                position: position,
                dimensions: {
                    x: 0.07,
                    y: 0.17,
                    z: 0.07
                },
                collisionsWillMove: true,
                shapeType: 'box',
                gravity: {
                    x: 0,
                    y: -0.5,
                    z: 0
                },
                velocity: {
                    x: 0,
                    y: -1,
                    z: 0
                },
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true
                    },
                    grabbableKey: {
                        invertSolidWhileHeld: true
                    }
                })
            });

        }

        function createPottedPlant(position) {
            var modelURL = "http://hifi-public.s3.amazonaws.com/models/potted_plant/potted_plant.fbx";

            var entity = Entities.addEntity({
                type: "Model",
                name: "Potted Plant",
                modelURL: modelURL,
                position: position,
                dimensions: {
                    x: 1.10,
                    y: 2.18,
                    z: 1.07
                },
                collisionsWillMove: true,
                shapeType: 'box',
                gravity: {
                    x: 0,
                    y: -9.8,
                    z: 0
                },
                velocity: {
                    x: 0,
                    y: 0,
                    z: 0
                },
                linearDamping: 0.4,
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true
                    },
                    grabbableKey: {
                        grabbable: false
                    }
                })
            });
        }


        function createCombinedArmChair(position) {
            var modelURL = "http://hifi-public.s3.amazonaws.com/models/red_arm_chair/combined_chair.fbx";
            var RED_ARM_CHAIR_COLLISION_HULL = "http://hifi-public.s3.amazonaws.com/models/red_arm_chair/red_arm_chair_collision_hull.obj";

            var rotation = Quat.fromPitchYawRollDegrees(0, -143, 0);

            var entity = Entities.addEntity({
                type: "Model",
                name: "Red Arm Chair",
                modelURL: modelURL,
                shapeType: 'compound',
                compoundShapeURL: RED_ARM_CHAIR_COLLISION_HULL,
                position: position,
                rotation: rotation,
                dimensions: {
                    x: 1.26,
                    y: 1.56,
                    z: 1.35
                },
                collisionsWillMove: true,
                gravity: {
                    x: 0,
                    y: -0.8,
                    z: 0
                },
                velocity: {
                    x: 0,
                    y: 0,
                    z: 0
                },
                linearDamping: 0.2,
                userData: JSON.stringify({
                    resetMe: {
                        resetMe: true
                    },
                    grabbableKey: {
                        grabbable: false
                    }
                })
            });

        }

        function createBlocks(position) {
            var baseURL = HIFI_PUBLIC_BUCKET + "models/content/planky/";
            var collisionSoundURL = "https://hifi-public.s3.amazonaws.com/sounds/Collisions-otherorganic/ToyWoodBlock.L.wav";
            var NUM_BLOCKS_PER_COLOR = 4;
            var i, j;

            var blockTypes = [{
                url: "planky_blue.fbx",
                dimensions: {
                    x: 0.05,
                    y: 0.05,
                    z: 0.25
                }
            }, {
                url: "planky_green.fbx",
                dimensions: {
                    x: 0.1,
                    y: 0.1,
                    z: 0.25
                }
            }, {
                url: "planky_natural.fbx",
                dimensions: {
                    x: 0.05,
                    y: 0.05,
                    z: 0.05
                }
            }, {
                url: "planky_yellow.fbx",
                dimensions: {
                    x: 0.03,
                    y: 0.05,
                    z: 0.25
                }
            }, {
                url: "planky_red.fbx",
                dimensions: {
                    x: 0.1,
                    y: 0.05,
                    z: 0.25
                }
            }];

            var modelURL, entity;
            for (i = 0; i < blockTypes.length; i++) {
                for (j = 0; j < NUM_BLOCKS_PER_COLOR; j++) {
                    modelURL = baseURL + blockTypes[i].url;
                    entity = Entities.addEntity({
                        type: "Model",
                        modelURL: modelURL,
                        position: Vec3.sum(position, {
                            x: j / 10,
                            y: i / 10,
                            z: 0
                        }),
                        shapeType: 'box',
                        name: "block",
                        dimensions: blockTypes[i].dimensions,
                        collisionsWillMove: true,
                        collisionSoundURL: collisionSoundURL,
                        gravity: {
                            x: 0,
                            y: -2.5,
                            z: 0
                        },
                        velocity: {
                            x: 0,
                            y: -0.01,
                            z: 0
                        },
                        userData: JSON.stringify({
                            resetMe: {
                                resetMe: true
                            }
                        })
                    });

                }
            }
        }

        function cleanup() {
            deleteAllToys();
        }

        if (shouldDeleteOnEndScript) {

            Script.scriptEnding.connect(cleanup);
        }
    };
    // entity scripts always need to return a newly constructed object of our type
    return new ResetSwitch();
});