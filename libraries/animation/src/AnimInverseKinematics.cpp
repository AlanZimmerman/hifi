//
//  AnimInverseKinematics.cpp
//
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AnimInverseKinematics.h"

#include <GeometryUtil.h>
#include <GLMHelpers.h>
#include <NumericalConstants.h>
#include <SharedUtil.h>

#include "ElbowConstraint.h"
#include "SwingTwistConstraint.h"
#include "AnimationLogging.h"

AnimInverseKinematics::AnimInverseKinematics(const QString& id) : AnimNode(AnimNode::Type::InverseKinematics, id) {
}

AnimInverseKinematics::~AnimInverseKinematics() {
    clearConstraints();
}

void AnimInverseKinematics::loadDefaultPoses(const AnimPoseVec& poses) {
    _defaultRelativePoses = poses;
    assert(_skeleton && _skeleton->getNumJoints() == (int)poses.size());
}

void AnimInverseKinematics::loadPoses(const AnimPoseVec& poses) {
    assert(_skeleton && ((poses.size() == 0) || (_skeleton->getNumJoints() == (int)poses.size())));
    if (_skeleton->getNumJoints() == (int)poses.size()) {
        _relativePoses = poses;
        _accumulators.resize(_relativePoses.size());
    } else {
        _relativePoses.clear();
        _accumulators.clear();
    }
}

void AnimInverseKinematics::computeAbsolutePoses(AnimPoseVec& absolutePoses) const {
    int numJoints = (int)_relativePoses.size();
    assert(numJoints <= _skeleton->getNumJoints());
    assert(numJoints == (int)absolutePoses.size());
    for (int i = 0; i < numJoints; ++i) {
        int parentIndex = _skeleton->getParentIndex(i);
        if (parentIndex < 0) {
            absolutePoses[i] = _relativePoses[i];
        } else {
            absolutePoses[i] = absolutePoses[parentIndex] * _relativePoses[i];
        }
    }
}

void AnimInverseKinematics::setTargetVars(
        const QString& jointName,
        const QString& positionVar,
        const QString& rotationVar,
        const QString& typeVar) {
    // if there are dups, last one wins.
    bool found = false;
    for (auto& targetVar: _targetVarVec) {
        if (targetVar.jointName == jointName) {
            // update existing targetVar
            targetVar.positionVar = positionVar;
            targetVar.rotationVar = rotationVar;
            targetVar.typeVar = typeVar;
            found = true;
            break;
        }
    }
    if (!found) {
        // create a new entry
        _targetVarVec.push_back(IKTargetVar(jointName, positionVar, rotationVar, typeVar));
    }
}

void AnimInverseKinematics::computeTargets(const AnimVariantMap& animVars, std::vector<IKTarget>& targets, const AnimPoseVec& underPoses) {
    // build a list of valid targets from _targetVarVec and animVars
    _maxTargetIndex = -1;
    bool removeUnfoundJoints = false;
    for (auto& targetVar : _targetVarVec) {
        if (targetVar.jointIndex == -1) {
            // this targetVar hasn't been validated yet...
            int jointIndex = _skeleton->nameToJointIndex(targetVar.jointName);
            if (jointIndex >= 0) {
                // this targetVar has a valid joint --> cache the indices
                targetVar.jointIndex = jointIndex;
            } else {
                qCWarning(animation) << "AnimInverseKinematics could not find jointName" << targetVar.jointName << "in skeleton";
                removeUnfoundJoints = true;
            }
        } else {
            IKTarget target;
            target.setType(animVars.lookup(targetVar.typeVar, (int)IKTarget::Type::RotationAndPosition));
            if (target.getType() != IKTarget::Type::Unknown) {
                AnimPose defaultPose = _skeleton->getAbsolutePose(targetVar.jointIndex, underPoses);
                glm::quat rotation = animVars.lookup(targetVar.rotationVar, defaultPose.rot);
                glm::vec3 translation = animVars.lookup(targetVar.positionVar, defaultPose.trans);
                if (target.getType() == IKTarget::Type::HipsRelativeRotationAndPosition) {
                    translation += _hipsOffset;
                }
                target.setPose(rotation, translation);
                target.setIndex(targetVar.jointIndex);
                targets.push_back(target);
                if (targetVar.jointIndex > _maxTargetIndex) {
                    _maxTargetIndex = targetVar.jointIndex;
                }
            }
        }
    }

    if (removeUnfoundJoints) {
        int numVars = _targetVarVec.size();
        int i = 0;
        while (i < numVars) {
            if (_targetVarVec[i].jointIndex == -1) {
                if (numVars > 1) {
                    // swap i for last element
                    _targetVarVec[i] = _targetVarVec[numVars - 1];
                }
                _targetVarVec.pop_back();
                --numVars;
            } else {
                ++i;
            }
        }
    }
}

void AnimInverseKinematics::solveWithCyclicCoordinateDescent(const std::vector<IKTarget>& targets) {
    // compute absolute poses that correspond to relative target poses
    AnimPoseVec absolutePoses;
    absolutePoses.resize(_relativePoses.size());
    computeAbsolutePoses(absolutePoses);

    // clear the accumulators before we start the IK solver
    for (auto& accumulator: _accumulators) {
        accumulator.clearAndClean();
    }

    int numLoops = 0;
    const int MAX_IK_LOOPS = 4;
    do {
        int lowestMovedIndex = _relativePoses.size();
        for (auto& target: targets) {
            IKTarget::Type targetType = target.getType();
            if (targetType == IKTarget::Type::RotationOnly) {
                // the final rotation will be enforced after the iterations
                continue;
            }

            // cache tip absolute transform
            int tipIndex = target.getIndex();
            int pivotIndex = _skeleton->getParentIndex(tipIndex);
            if (pivotIndex == -1) {
                continue;
            }
            int pivotsParentIndex = _skeleton->getParentIndex(pivotIndex);
            if (pivotsParentIndex == -1) {
                // TODO?: handle case where tip's parent is root?
                continue;
            }

            glm::vec3 tipPosition = absolutePoses[tipIndex].trans;
            glm::quat tipRotation = absolutePoses[tipIndex].rot;

            // cache tip's parent's absolute rotation so we can recompute the tip's parent-relative
            // as we proceed walking down the joint chain
            glm::quat tipParentRotation = absolutePoses[pivotIndex].rot;

            // descend toward root, pivoting each joint to get tip closer to target
            while (pivotsParentIndex != -1) {
                // compute the two lines that should be aligned
                glm::vec3 jointPosition = absolutePoses[pivotIndex].trans;
                glm::vec3 leverArm = tipPosition - jointPosition;

                glm::quat deltaRotation;
                if (targetType == IKTarget::Type::RotationAndPosition ||
                        targetType == IKTarget::Type::HipsRelativeRotationAndPosition) {
                    // compute the swing that would get get tip closer
                    glm::vec3 targetLine = target.getTranslation() - jointPosition;
                    glm::vec3 axis = glm::cross(leverArm, targetLine);
                    float axisLength = glm::length(axis);
                    const float MIN_AXIS_LENGTH = 1.0e-4f;
                    if (axisLength > MIN_AXIS_LENGTH) {
                        // compute deltaRotation for alignment (swings tip closer to target)
                        axis /= axisLength;
                        float angle = acosf(glm::dot(leverArm, targetLine) / (glm::length(leverArm) * glm::length(targetLine)));

                        // NOTE: even when axisLength is not zero (e.g. lever-arm and pivot-arm are not quite aligned) it is
                        // still possible for the angle to be zero so we also check that to avoid unnecessary calculations.
                        const float MIN_ADJUSTMENT_ANGLE = 1.0e-4f;
                        if (angle > MIN_ADJUSTMENT_ANGLE) {
                            // reduce angle by a fraction (for stability)
                            const float fraction = 0.5f;
                            angle *= fraction;
                            deltaRotation = glm::angleAxis(angle, axis);

                            // The swing will re-orient the tip but there will tend to be be a non-zero delta between the tip's
                            // new rotation and its target.  This is the final parent-relative rotation that the tip joint have
                            // make to achieve its target rotation.
                            glm::quat tipRelativeRotation = glm::inverse(deltaRotation * tipParentRotation) * target.getRotation();

                            // enforce tip's constraint
                            RotationConstraint* constraint = getConstraint(tipIndex);
                            if (constraint) {
                                bool constrained = constraint->apply(tipRelativeRotation);
                                if (constrained) {
                                    // The tip's final parent-relative rotation would violate its constraint
                                    // so we try to pre-twist this pivot to compensate.
                                    glm::quat constrainedTipRotation = deltaRotation * tipParentRotation * tipRelativeRotation;
                                    glm::quat missingRotation = target.getRotation() * glm::inverse(constrainedTipRotation);
                                    glm::quat swingPart;
                                    glm::quat twistPart;
                                    glm::vec3 axis = glm::normalize(deltaRotation * leverArm);
                                    swingTwistDecomposition(missingRotation, axis, swingPart, twistPart);
                                    float dotSign = copysignf(1.0f, twistPart.w);
                                    deltaRotation = glm::normalize(glm::lerp(glm::quat(), dotSign * twistPart, fraction)) * deltaRotation;
                                }
                            }
                        }
                    }
                } else if (targetType == IKTarget::Type::HmdHead) {
                    // An HmdHead target slaves the orientation of the end-effector by distributing rotation
                    // deltas up the hierarchy.  Its target position is enforced later by shifting the hips.
                    deltaRotation = target.getRotation() * glm::inverse(tipRotation);
                    float dotSign = copysignf(1.0f, deltaRotation.w);
                    const float ANGLE_DISTRIBUTION_FACTOR = 0.15f;
                    deltaRotation = glm::normalize(glm::lerp(glm::quat(), dotSign * deltaRotation, ANGLE_DISTRIBUTION_FACTOR));
                }

                // compute joint's new parent-relative rotation after swing
                // Q' = dQ * Q   and   Q = Qp * q   -->   q' = Qp^ * dQ * Q
                glm::quat newRot = glm::normalize(glm::inverse(
                        absolutePoses[pivotsParentIndex].rot) *
                        deltaRotation *
                        absolutePoses[pivotIndex].rot);

                // enforce pivot's constraint
                RotationConstraint* constraint = getConstraint(pivotIndex);
                if (constraint) {
                    bool constrained = constraint->apply(newRot);
                    if (constrained) {
                        // the constraint will modify the movement of the tip so we have to compute the modified
                        // model-frame deltaRotation
                        // Q' = Qp^ * dQ * Q  -->  dQ =   Qp * Q' * Q^
                        deltaRotation = absolutePoses[pivotsParentIndex].rot *
                            newRot *
                            glm::inverse(absolutePoses[pivotIndex].rot);
                    }
                }

                // store the rotation change in the accumulator
                _accumulators[pivotIndex].add(newRot, target.getWeight());

                // this joint has been changed so we check to see if it has the lowest index
                if (pivotIndex < lowestMovedIndex) {
                    lowestMovedIndex = pivotIndex;
                }

                // keep track of tip's new transform as we descend towards root
                tipPosition = jointPosition + deltaRotation * leverArm;
                tipRotation = glm::normalize(deltaRotation * tipRotation);
                tipParentRotation = glm::normalize(deltaRotation * tipParentRotation);

                pivotIndex = pivotsParentIndex;
                pivotsParentIndex = _skeleton->getParentIndex(pivotIndex);
            }
        }
        ++numLoops;

        // harvest accumulated rotations and apply the average
        const int numJoints = (int)_accumulators.size();
        for (int i = 0; i < numJoints; ++i) {
            if (_accumulators[i].size() > 0) {
                _relativePoses[i].rot = _accumulators[i].getAverage();
                _accumulators[i].clear();
            }
        }

        // only update the absolutePoses that need it: those between lowestMovedIndex and _maxTargetIndex
        for (int i = lowestMovedIndex; i <= _maxTargetIndex; ++i) {
            int parentIndex = _skeleton->getParentIndex(i);
            if (parentIndex != -1) {
                absolutePoses[i] = absolutePoses[parentIndex] * _relativePoses[i];
            }
        }
    } while (numLoops < MAX_IK_LOOPS);

    // finally set the relative rotation of each tip to agree with absolute target rotation
    for (auto& target: targets) {
        int tipIndex = target.getIndex();
        int parentIndex = _skeleton->getParentIndex(tipIndex);
        if (parentIndex != -1) {
            const glm::quat& targetRotation = target.getRotation();
            // compute tip's new parent-relative rotation
            // Q = Qp * q   -->   q' = Qp^ * Q
            glm::quat newRelativeRotation = glm::inverse(absolutePoses[parentIndex].rot) * targetRotation;
            RotationConstraint* constraint = getConstraint(tipIndex);
            if (constraint) {
                constraint->apply(newRelativeRotation);
                // TODO: ATM the final rotation target just fails but we need to provide
                // feedback to the IK system so that it can adjust the bones up the skeleton
                // to help this rotation target get met.
            }
            _relativePoses[tipIndex].rot = newRelativeRotation;
            absolutePoses[tipIndex].rot = targetRotation;
        }
    }
}

//virtual
const AnimPoseVec& AnimInverseKinematics::evaluate(const AnimVariantMap& animVars, float dt, AnimNode::Triggers& triggersOut) {
    // don't call this function, call overlay() instead
    assert(false);
    return _relativePoses;
}

//virtual
const AnimPoseVec& AnimInverseKinematics::overlay(const AnimVariantMap& animVars, float dt, Triggers& triggersOut, const AnimPoseVec& underPoses) {
    if (_relativePoses.size() != underPoses.size()) {
        loadPoses(underPoses);
    } else {
        // relax toward underpose
        // HACK: this relaxation needs to be constant per-frame rather than per-realtime
        // in order to prevent IK "flutter" for bad FPS.  The bad news is that the good parts
        // of this relaxation will be FPS dependent (low FPS will make the limbs align slower
        // in real-time), however most people will not notice this and this problem is less
        // annoying than the flutter.
        const float blend = (1.0f / 60.0f) / (0.25f); // effectively: dt / RELAXATION_TIMESCALE
        int numJoints = (int)_relativePoses.size();
        for (int i = 0; i < numJoints; ++i) {
            float dotSign = copysignf(1.0f, glm::dot(_relativePoses[i].rot, underPoses[i].rot));
            if (_accumulators[i].isDirty()) {
                _relativePoses[i].rot = glm::normalize(glm::lerp(_relativePoses[i].rot, dotSign * underPoses[i].rot, blend));
            } else {
                _relativePoses[i].rot = underPoses[i].rot;
            }
            _relativePoses[i].trans = underPoses[i].trans;
        }
    }

    if (!_relativePoses.empty()) {
        // build a list of targets from _targetVarVec
        std::vector<IKTarget> targets;
        computeTargets(animVars, targets, underPoses);

        if (targets.empty()) {
            // no IK targets but still need to enforce constraints
            std::map<int, RotationConstraint*>::iterator constraintItr = _constraints.begin();
            while (constraintItr != _constraints.end()) {
                int index = constraintItr->first;
                glm::quat rotation = _relativePoses[index].rot;
                constraintItr->second->apply(rotation);
                _relativePoses[index].rot = rotation;
                ++constraintItr;
            }
        } else {
            // shift the hips according to the offset from the previous frame
            float offsetLength = glm::length(_hipsOffset);
            const float MIN_HIPS_OFFSET_LENGTH = 0.03f;
            if (offsetLength > MIN_HIPS_OFFSET_LENGTH) {
                // but only if offset is long enough
                float scaleFactor = ((offsetLength - MIN_HIPS_OFFSET_LENGTH) / offsetLength);
                _relativePoses[_hipsIndex].trans = underPoses[_hipsIndex].trans + scaleFactor * _hipsOffset;
            }

            solveWithCyclicCoordinateDescent(targets);

            // compute the new target hips offset (for next frame)
            // by looking for discrepancies between where a targeted endEffector is
            // and where it wants to be (after IK solutions are done)
            glm::vec3 newHipsOffset = Vectors::ZERO;
            for (auto& target: targets) {
                int targetIndex = target.getIndex();
                if (targetIndex == _headIndex && _headIndex != -1) {
                    // special handling for headTarget
                    if (target.getType() == IKTarget::Type::RotationOnly) {
                        // we want to shift the hips to bring the underpose closer
                        // to where the head happens to be (overpose)
                        glm::vec3 under = _skeleton->getAbsolutePose(_headIndex, underPoses).trans;
                        glm::vec3 actual = _skeleton->getAbsolutePose(_headIndex, _relativePoses).trans;
                        const float HEAD_OFFSET_SLAVE_FACTOR = 0.65f;
                        newHipsOffset += HEAD_OFFSET_SLAVE_FACTOR * (actual - under);
                    } else if (target.getType() == IKTarget::Type::HmdHead) {
                        // we want to shift the hips to bring the head to its designated position
                        glm::vec3 actual = _skeleton->getAbsolutePose(_headIndex, _relativePoses).trans;
                        _hipsOffset += target.getTranslation() - actual;
                        // and ignore all other targets
                        newHipsOffset = _hipsOffset;
                        break;
                    }
                } else if (target.getType() == IKTarget::Type::RotationAndPosition) {
                    glm::vec3 actualPosition = _skeleton->getAbsolutePose(targetIndex, _relativePoses).trans;
                    glm::vec3 targetPosition = target.getTranslation();
                    newHipsOffset += targetPosition - actualPosition;
                }
            }

            // smooth transitions by relaxing _hipsOffset toward the new value
            const float HIPS_OFFSET_SLAVE_TIMESCALE = 0.15f;
            _hipsOffset += (newHipsOffset - _hipsOffset) * (dt / HIPS_OFFSET_SLAVE_TIMESCALE);
        }
    }
    return _relativePoses;
}

RotationConstraint* AnimInverseKinematics::getConstraint(int index) {
    RotationConstraint* constraint = nullptr;
    std::map<int, RotationConstraint*>::iterator constraintItr = _constraints.find(index);
    if (constraintItr != _constraints.end()) {
        constraint = constraintItr->second;
    }
    return constraint;
}

void AnimInverseKinematics::clearConstraints() {
    std::map<int, RotationConstraint*>::iterator constraintItr = _constraints.begin();
    while (constraintItr != _constraints.end()) {
        delete constraintItr->second;
        ++constraintItr;
    }
    _constraints.clear();
}

void AnimInverseKinematics::initConstraints() {
    if (!_skeleton) {
        return;
    }
    // We create constraints for the joints shown here
    // (and their Left counterparts if applicable).
    //
    //
    //                                    O RightHand
    //                      Head         /
    //                          O       /
    //                      Neck|      O RightForeArm
    //                          O     /
    //                        O | O  / RightShoulder
    //      O-------O-------O' \|/ 'O
    //                   Spine2 O  RightArm
    //                          |
    //                          |
    //                   Spine1 O
    //                          |
    //                          |
    //                    Spine O
    //         y                |
    //         |                |
    //         |            O---O---O RightUpLeg
    //      z  |            | Hips2 |
    //       \ |            |       |
    //        \|            |       |
    //  x -----+            O       O RightLeg
    //                      |       |
    //                      |       |
    //                      |       |
    //                      O       O RightFoot
    //                     /       /
    //                 O--O    O--O

    loadDefaultPoses(_skeleton->getRelativeBindPoses());

    // compute corresponding absolute poses
    int numJoints = (int)_defaultRelativePoses.size();
    AnimPoseVec absolutePoses;
    absolutePoses.resize(numJoints);
    for (int i = 0; i < numJoints; ++i) {
        int parentIndex = _skeleton->getParentIndex(i);
        if (parentIndex < 0) {
            absolutePoses[i] = _defaultRelativePoses[i];
        } else {
            absolutePoses[i] = absolutePoses[parentIndex] * _defaultRelativePoses[i];
        }
    }

    clearConstraints();
    for (int i = 0; i < numJoints; ++i) {
        // compute the joint's baseName and remember whether its prefix was "Left" or not
        QString baseName = _skeleton->getJointName(i);
        bool isLeft = baseName.startsWith("Left", Qt::CaseInsensitive);
        float mirror = isLeft ? -1.0f : 1.0f;
        if (isLeft) {
            baseName.remove(0, 4);
        } else if (baseName.startsWith("Right", Qt::CaseInsensitive)) {
            baseName.remove(0, 5);
        }

        RotationConstraint* constraint = nullptr;
        if (0 == baseName.compare("Arm", Qt::CaseInsensitive)) {
            SwingTwistConstraint* stConstraint = new SwingTwistConstraint();
            stConstraint->setReferenceRotation(_defaultRelativePoses[i].rot);
            stConstraint->setTwistLimits(-PI / 2.0f, PI / 2.0f);

            /* KEEP THIS CODE for future experimentation
            // these directions are approximate swing limits in root-frame
            // NOTE: they don't need to be normalized
            std::vector<glm::vec3> swungDirections;
            swungDirections.push_back(glm::vec3(mirror * 1.0f, 1.0f, 1.0f));
            swungDirections.push_back(glm::vec3(mirror * 1.0f, 0.0f, 1.0f));
            swungDirections.push_back(glm::vec3(mirror * 1.0f, -1.0f, 0.5f));
            swungDirections.push_back(glm::vec3(mirror * 0.0f, -1.0f, 0.0f));
            swungDirections.push_back(glm::vec3(mirror * 0.0f, -1.0f, -1.0f));
            swungDirections.push_back(glm::vec3(mirror * -0.5f, 0.0f, -1.0f));
            swungDirections.push_back(glm::vec3(mirror * 0.0f, 1.0f, -1.0f));
            swungDirections.push_back(glm::vec3(mirror * 0.0f, 1.0f, 0.0f));

            // rotate directions into joint-frame
            glm::quat invAbsoluteRotation = glm::inverse(absolutePoses[i].rot);
            int numDirections = (int)swungDirections.size();
            for (int j = 0; j < numDirections; ++j) {
                swungDirections[j] = invAbsoluteRotation * swungDirections[j];
            }
            stConstraint->setSwingLimits(swungDirections);
            */

            // simple cone
            std::vector<float> minDots;
            const float MAX_HAND_SWING = PI / 2.0f;
            minDots.push_back(cosf(MAX_HAND_SWING));
            stConstraint->setSwingLimits(minDots);

            constraint = static_cast<RotationConstraint*>(stConstraint);
        } else if (0 == baseName.compare("UpLeg", Qt::CaseInsensitive)) {
            SwingTwistConstraint* stConstraint = new SwingTwistConstraint();
            stConstraint->setReferenceRotation(_defaultRelativePoses[i].rot);
            stConstraint->setTwistLimits(-PI / 4.0f, PI / 4.0f);

            // these directions are approximate swing limits in root-frame
            // NOTE: they don't need to be normalized
            std::vector<glm::vec3> swungDirections;
            swungDirections.push_back(glm::vec3(mirror * 0.25f, 0.0f, 1.0f));
            swungDirections.push_back(glm::vec3(mirror * -0.5f, 0.0f, 1.0f));
            swungDirections.push_back(glm::vec3(mirror * -1.0f, 0.0f, 1.0f));
            swungDirections.push_back(glm::vec3(mirror * -1.0f, 0.0f, 0.0f));
            swungDirections.push_back(glm::vec3(mirror * -0.5f, -0.5f, -1.0f));
            swungDirections.push_back(glm::vec3(mirror * 0.0f, -0.75f, -1.0f));
            swungDirections.push_back(glm::vec3(mirror * 0.25f, -1.0f, 0.0f));
            swungDirections.push_back(glm::vec3(mirror * 0.25f, -1.0f, 1.0f));

            // rotate directions into joint-frame
            glm::quat invAbsoluteRotation = glm::inverse(absolutePoses[i].rot);
            int numDirections = (int)swungDirections.size();
            for (int j = 0; j < numDirections; ++j) {
                swungDirections[j] = invAbsoluteRotation * swungDirections[j];
            }
            stConstraint->setSwingLimits(swungDirections);

            constraint = static_cast<RotationConstraint*>(stConstraint);
        } else if (0 == baseName.compare("Hand", Qt::CaseInsensitive)) {
            SwingTwistConstraint* stConstraint = new SwingTwistConstraint();
            stConstraint->setReferenceRotation(_defaultRelativePoses[i].rot);
            const float MAX_HAND_TWIST = 3.0f * PI / 5.0f;
            const float MIN_HAND_TWIST = -PI / 2.0f;
            if (isLeft) {
                stConstraint->setTwistLimits(-MAX_HAND_TWIST, -MIN_HAND_TWIST);
            } else {
                stConstraint->setTwistLimits(MIN_HAND_TWIST, MAX_HAND_TWIST);
            }

            /* KEEP THIS CODE for future experimentation
             * a more complicated wrist with asymmetric cone
            // these directions are approximate swing limits in parent-frame
            // NOTE: they don't need to be normalized
            std::vector<glm::vec3> swungDirections;
            swungDirections.push_back(glm::vec3(1.0f, 1.0f, 0.0f));
            swungDirections.push_back(glm::vec3(0.75f, 1.0f, -1.0f));
            swungDirections.push_back(glm::vec3(-0.75f, 1.0f, -1.0f));
            swungDirections.push_back(glm::vec3(-1.0f, 1.0f, 0.0f));
            swungDirections.push_back(glm::vec3(-0.75f, 1.0f, 1.0f));
            swungDirections.push_back(glm::vec3(0.75f, 1.0f, 1.0f));

            // rotate directions into joint-frame
            glm::quat invRelativeRotation = glm::inverse(_defaultRelativePoses[i].rot);
            int numDirections = (int)swungDirections.size();
            for (int j = 0; j < numDirections; ++j) {
                swungDirections[j] = invRelativeRotation * swungDirections[j];
            }
            stConstraint->setSwingLimits(swungDirections);
            */

            // simple cone
            std::vector<float> minDots;
            const float MAX_HAND_SWING = PI / 2.0f;
            minDots.push_back(cosf(MAX_HAND_SWING));
            stConstraint->setSwingLimits(minDots);

            constraint = static_cast<RotationConstraint*>(stConstraint);
        } else if (baseName.startsWith("Shoulder", Qt::CaseInsensitive)) {
            SwingTwistConstraint* stConstraint = new SwingTwistConstraint();
            stConstraint->setReferenceRotation(_defaultRelativePoses[i].rot);
            const float MAX_SHOULDER_TWIST = PI / 20.0f;
            stConstraint->setTwistLimits(-MAX_SHOULDER_TWIST, MAX_SHOULDER_TWIST);

            std::vector<float> minDots;
            const float MAX_SHOULDER_SWING = PI / 20.0f;
            minDots.push_back(cosf(MAX_SHOULDER_SWING));
            stConstraint->setSwingLimits(minDots);

            constraint = static_cast<RotationConstraint*>(stConstraint);
        } else if (baseName.startsWith("Spine", Qt::CaseInsensitive)) {
            SwingTwistConstraint* stConstraint = new SwingTwistConstraint();
            stConstraint->setReferenceRotation(_defaultRelativePoses[i].rot);
            const float MAX_SPINE_TWIST = PI / 8.0f;
            stConstraint->setTwistLimits(-MAX_SPINE_TWIST, MAX_SPINE_TWIST);

            std::vector<float> minDots;
            const float MAX_SPINE_SWING = PI / 14.0f;
            minDots.push_back(cosf(MAX_SPINE_SWING));
            stConstraint->setSwingLimits(minDots);

            constraint = static_cast<RotationConstraint*>(stConstraint);
        } else if (baseName.startsWith("Hips2", Qt::CaseInsensitive)) {
            SwingTwistConstraint* stConstraint = new SwingTwistConstraint();
            stConstraint->setReferenceRotation(_defaultRelativePoses[i].rot);
            const float MAX_SPINE_TWIST = PI / 8.0f;
            stConstraint->setTwistLimits(-MAX_SPINE_TWIST, MAX_SPINE_TWIST);

            std::vector<float> minDots;
            const float MAX_SPINE_SWING = PI / 14.0f;
            minDots.push_back(cosf(MAX_SPINE_SWING));
            stConstraint->setSwingLimits(minDots);

            constraint = static_cast<RotationConstraint*>(stConstraint);
        } else if (0 == baseName.compare("Neck", Qt::CaseInsensitive)) {
            SwingTwistConstraint* stConstraint = new SwingTwistConstraint();
            stConstraint->setReferenceRotation(_defaultRelativePoses[i].rot);
            const float MAX_NECK_TWIST = PI / 4.0f;
            stConstraint->setTwistLimits(-MAX_NECK_TWIST, MAX_NECK_TWIST);

            std::vector<float> minDots;
            const float MAX_NECK_SWING = PI / 3.0f;
            minDots.push_back(cosf(MAX_NECK_SWING));
            stConstraint->setSwingLimits(minDots);

            constraint = static_cast<RotationConstraint*>(stConstraint);
        } else if (0 == baseName.compare("Head", Qt::CaseInsensitive)) {
            SwingTwistConstraint* stConstraint = new SwingTwistConstraint();
            stConstraint->setReferenceRotation(_defaultRelativePoses[i].rot);
            const float MAX_HEAD_TWIST = PI / 4.0f;
            stConstraint->setTwistLimits(-MAX_HEAD_TWIST, MAX_HEAD_TWIST);

            std::vector<float> minDots;
            const float MAX_HEAD_SWING = PI / 4.0f;
            minDots.push_back(cosf(MAX_HEAD_SWING));
            stConstraint->setSwingLimits(minDots);

            constraint = static_cast<RotationConstraint*>(stConstraint);
        } else if (0 == baseName.compare("ForeArm", Qt::CaseInsensitive)) {
            // The elbow joint rotates about the parent-frame's zAxis (-zAxis) for the Right (Left) arm.
            ElbowConstraint* eConstraint = new ElbowConstraint();
            glm::quat referenceRotation = _defaultRelativePoses[i].rot;
            eConstraint->setReferenceRotation(referenceRotation);

            // we determine the max/min angles by rotating the swing limit lines from parent- to child-frame
            // then measure the angles to swing the yAxis into alignment
            glm::vec3 hingeAxis = - mirror * Vectors::UNIT_Z;
            const float MIN_ELBOW_ANGLE = 0.05f;
            const float MAX_ELBOW_ANGLE = 11.0f * PI / 12.0f;
            glm::quat invReferenceRotation = glm::inverse(referenceRotation);
            glm::vec3 minSwingAxis = invReferenceRotation * glm::angleAxis(MIN_ELBOW_ANGLE, hingeAxis) * Vectors::UNIT_Y;
            glm::vec3 maxSwingAxis = invReferenceRotation * glm::angleAxis(MAX_ELBOW_ANGLE, hingeAxis) * Vectors::UNIT_Y;

            // for the rest of the math we rotate hingeAxis into the child frame
            hingeAxis = referenceRotation * hingeAxis;
            eConstraint->setHingeAxis(hingeAxis);

            glm::vec3 projectedYAxis = glm::normalize(Vectors::UNIT_Y - glm::dot(Vectors::UNIT_Y, hingeAxis) * hingeAxis);
            float minAngle = acosf(glm::dot(projectedYAxis, minSwingAxis));
            if (glm::dot(hingeAxis, glm::cross(projectedYAxis, minSwingAxis)) < 0.0f) {
                minAngle = - minAngle;
            }
            float maxAngle = acosf(glm::dot(projectedYAxis, maxSwingAxis));
            if (glm::dot(hingeAxis, glm::cross(projectedYAxis, maxSwingAxis)) < 0.0f) {
                maxAngle = - maxAngle;
            }
            eConstraint->setAngleLimits(minAngle, maxAngle);

            constraint = static_cast<RotationConstraint*>(eConstraint);
        } else if (0 == baseName.compare("Leg", Qt::CaseInsensitive)) {
            // The knee joint rotates about the parent-frame's -xAxis.
            ElbowConstraint* eConstraint = new ElbowConstraint();
            glm::quat referenceRotation = _defaultRelativePoses[i].rot;
            eConstraint->setReferenceRotation(referenceRotation);
            glm::vec3 hingeAxis = -1.0f * Vectors::UNIT_X;

            // we determine the max/min angles by rotating the swing limit lines from parent- to child-frame
            // then measure the angles to swing the yAxis into alignment
            const float MIN_KNEE_ANGLE = 0.0f;
            const float MAX_KNEE_ANGLE = 3.0f * PI / 4.0f;
            glm::quat invReferenceRotation = glm::inverse(referenceRotation);
            glm::vec3 minSwingAxis = invReferenceRotation * glm::angleAxis(MIN_KNEE_ANGLE, hingeAxis) * Vectors::UNIT_Y;
            glm::vec3 maxSwingAxis = invReferenceRotation * glm::angleAxis(MAX_KNEE_ANGLE, hingeAxis) * Vectors::UNIT_Y;

            // for the rest of the math we rotate hingeAxis into the child frame
            hingeAxis = referenceRotation * hingeAxis;
            eConstraint->setHingeAxis(hingeAxis);

            glm::vec3 projectedYAxis = glm::normalize(Vectors::UNIT_Y - glm::dot(Vectors::UNIT_Y, hingeAxis) * hingeAxis);
            float minAngle = acosf(glm::dot(projectedYAxis, minSwingAxis));
            if (glm::dot(hingeAxis, glm::cross(projectedYAxis, minSwingAxis)) < 0.0f) {
                minAngle = - minAngle;
            }
            float maxAngle = acosf(glm::dot(projectedYAxis, maxSwingAxis));
            if (glm::dot(hingeAxis, glm::cross(projectedYAxis, maxSwingAxis)) < 0.0f) {
                maxAngle = - maxAngle;
            }
            eConstraint->setAngleLimits(minAngle, maxAngle);

            constraint = static_cast<RotationConstraint*>(eConstraint);
        } else if (0 == baseName.compare("Foot", Qt::CaseInsensitive)) {
            SwingTwistConstraint* stConstraint = new SwingTwistConstraint();
            stConstraint->setReferenceRotation(_defaultRelativePoses[i].rot);
            stConstraint->setTwistLimits(-PI / 4.0f, PI / 4.0f);

            // these directions are approximate swing limits in parent-frame
            // NOTE: they don't need to be normalized
            std::vector<glm::vec3> swungDirections;
            swungDirections.push_back(Vectors::UNIT_Y);
            swungDirections.push_back(Vectors::UNIT_X);
            swungDirections.push_back(glm::vec3(1.0f, 1.0f, 1.0f));
            swungDirections.push_back(glm::vec3(1.0f, 1.0f, -1.0f));

            // rotate directions into joint-frame
            glm::quat invRelativeRotation = glm::inverse(_defaultRelativePoses[i].rot);
            int numDirections = (int)swungDirections.size();
            for (int j = 0; j < numDirections; ++j) {
                swungDirections[j] = invRelativeRotation * swungDirections[j];
            }
            stConstraint->setSwingLimits(swungDirections);

            constraint = static_cast<RotationConstraint*>(stConstraint);
        }
        if (constraint) {
            _constraints[i] = constraint;
        }
    }
}

void AnimInverseKinematics::setSkeletonInternal(AnimSkeleton::ConstPointer skeleton) {
    AnimNode::setSkeletonInternal(skeleton);

    // invalidate all targetVars
    for (auto& targetVar: _targetVarVec) {
        targetVar.jointIndex = -1;
    }

    _maxTargetIndex = -1;

    for (auto& accumulator: _accumulators) {
        accumulator.clearAndClean();
    }

    if (skeleton) {
        initConstraints();
        _headIndex = _skeleton->nameToJointIndex("Head");
        _hipsIndex = _skeleton->nameToJointIndex("Hips");
    } else {
        clearConstraints();
        _headIndex = -1;
        _hipsIndex = -1;
    }
}
