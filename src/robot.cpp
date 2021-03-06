#include <frankx/robot.hpp>


namespace frankx {

Robot::Robot(std::string fci_ip, double dynamic_rel): franka::Robot(fci_ip), fci_ip(fci_ip), velocity_rel(dynamic_rel), acceleration_rel(dynamic_rel), jerk_rel(dynamic_rel) {}

void Robot::setDefaultBehavior() {
    setCollisionBehavior(
        {{20.0, 20.0, 20.0, 20.0, 20.0, 20.0, 20.0}},
        {{20.0, 20.0, 20.0, 20.0, 20.0, 20.0, 20.0}},
        {{20.0, 20.0, 20.0, 20.0, 20.0, 20.0, 20.0}},
        {{20.0, 20.0, 20.0, 20.0, 20.0, 20.0, 20.0}},
        {{30.0, 30.0, 30.0, 30.0, 30.0, 30.0}},
        {{30.0, 30.0, 30.0, 30.0, 30.0, 30.0}},
        {{30.0, 30.0, 30.0, 30.0, 30.0, 30.0}},
        {{30.0, 30.0, 30.0, 30.0, 30.0, 30.0}}
    );

    // setJointImpedance({{3000, 3000, 3000, 2500, 2500, 2000, 2000}});
    // setCartesianImpedance({{3000, 3000, 3000, 300, 300, 300}});

    setEE({0.7071, 0.7071, 0.0, 0.0, 0.7071, -0.7071, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0});
}

void Robot::setDynamicRel(double dynamic_rel) {
    velocity_rel = dynamic_rel;
    acceleration_rel = dynamic_rel;
    jerk_rel = dynamic_rel;
}

bool Robot::hasErrors() {
    return bool(readOnce().current_errors);
}

bool Robot::recoverFromErrors() {
    automaticErrorRecovery();
    return !hasErrors();
}

Affine Robot::currentPose(const Affine& frame) {
    auto state = readOnce();
    return Affine(state.O_T_EE) * frame;
}

bool Robot::move(ImpedanceMotion motion) {
    return move(Affine(), motion);
}

bool Robot::move(ImpedanceMotion motion, MotionData& data) {
    return move(Affine(), motion, data);
}

bool Robot::move(const Affine& frame, ImpedanceMotion motion) {
    auto data = MotionData();
    return move(motion, data);
}

bool Robot::move(const Affine& frame, ImpedanceMotion motion, MotionData& data) {
    // motion.setDynamicRel(data.velocity_rel * velocity_rel, data.acceleration_rel * acceleration_rel, data.jerk_rel * jerk_rel);
    // try {
    //     control(motion);

    // } catch (franka::Exception exception) {
    //     std::cout << exception.what() << std::endl;
    //     return false;
    // }
    return true;
}

bool Robot::move(JointMotion motion) {
    return move(Affine(), motion);
}

bool Robot::move(JointMotion motion, MotionData& data) {
    return move(Affine(), motion, data);
}

bool Robot::move(const Affine& frame, JointMotion motion) {
    auto data = MotionData();
    return move(frame, motion, data);
}

bool Robot::move(const Affine& frame, JointMotion motion, MotionData& data) {
    motion.update(this, frame, data);
    try {
        control(motion);

    } catch (franka::Exception exception) {
        std::cout << exception.what() << std::endl;
        return false;
    }
    return true;
}

bool Robot::move(WaypointMotion motion) {
    return move(Affine(), motion);
}

bool Robot::move(WaypointMotion motion, MotionData& data) {
    return move(Affine(), motion, data);
}

bool Robot::move(const Affine& frame, WaypointMotion motion) {
    auto data = MotionData();
    return move(frame, motion, data);
}

bool Robot::move(const Affine& frame, WaypointMotion motion, MotionData& data) {
    const auto rml = std::make_shared<ReflexxesAPI>(degrees_of_freedoms, control_rate);
    auto input_parameters = std::make_shared<RMLPositionInputParameters>(degrees_of_freedoms);
    auto output_parameters = std::make_shared<RMLPositionOutputParameters>(degrees_of_freedoms);

    int result_value = 0;
    RMLPositionFlags flags;
    flags.SynchronizationBehavior = RMLPositionFlags::PHASE_SYNCHRONIZATION_IF_POSSIBLE;

    setVector(input_parameters->SelectionVector, VectorCartRotElbow(true, true, true));
    setInputLimits(input_parameters.get(), data);

    WaypointMotion current_motion = motion;
    auto waypoint_iterator = current_motion.waypoints.begin();
    bool waypoint_has_elbow = false;

    Vector7d old_vector = Vector7d::Zero();
    auto old_affine = Affine();
    double old_elbow = 0.0;

    double time = 0.0;

    auto motion_generator = [&](const franka::RobotState& robot_state, franka::Duration period) -> franka::CartesianPose {
        time += period.toSec();
        if (time == 0.0) {
            franka::CartesianPose initial_pose = franka::CartesianPose(robot_state.O_T_EE_c, robot_state.elbow_c);
            Vector7d initial_vector = Affine(initial_pose).vector_with_elbow(initial_pose.elbow[0]);
            Vector7d initial_velocity = (Vector7d() << robot_state.O_dP_EE_c[0], robot_state.O_dP_EE_c[1], robot_state.O_dP_EE_c[2], robot_state.O_dP_EE_c[3], robot_state.O_dP_EE_c[4], robot_state.O_dP_EE_c[5], robot_state.delbow_c[0]).finished();

            old_affine = Affine(initial_pose);
            old_vector = initial_vector;
            old_elbow = old_vector(6);

            setVector(input_parameters->CurrentPositionVector, initial_vector);
            setVector(input_parameters->CurrentVelocityVector, initial_velocity);
            setZero(input_parameters->CurrentAccelerationVector);

            const Waypoint current_waypoint = *waypoint_iterator;
            waypoint_has_elbow = current_waypoint.elbow.has_value();
            auto target_position_vector = current_waypoint.getTargetVector(frame, old_affine, old_elbow);

            setVector(input_parameters->SelectionVector, VectorCartRotElbow(true, true, waypoint_has_elbow));
            setInputLimits(input_parameters.get(), current_waypoint, data);
            setVector(input_parameters->TargetPositionVector, target_position_vector);
            setVector(input_parameters->TargetVelocityVector, current_waypoint.velocity);

            old_affine = current_waypoint.getTargetAffine(frame, old_affine);
            old_vector = target_position_vector;
            old_elbow = old_vector(6);
        }

        for (auto& reaction : data.reactions) {
            if (reaction.has_fired) {
                continue;
            }

            if (reaction.condition.callback(robot_state, time)) {
                std::cout << "reaction fired" << std::endl;
                reaction.has_fired = true;

                bool new_motion = false;

                if (reaction.waypoint_action.has_value()) {
                    new_motion = true;
                    current_motion = reaction.waypoint_action.value()(robot_state, time);
                } else if (reaction.waypoint_motion.has_value()) {
                    new_motion = true;
                    current_motion = *(reaction.waypoint_motion.value());
                }
#ifdef WITH_PYTHON
                else {
                    stop();
                }
#endif

                if (new_motion) {
                    waypoint_iterator = current_motion.waypoints.begin();

                    auto current_pose = franka::CartesianPose(robot_state.O_T_EE_c, robot_state.elbow_c);
                    auto current_vector = Affine(current_pose).vector_with_elbow(current_pose.elbow[0]);
                    old_affine = Affine(current_pose);
                    old_vector = current_vector;
                    old_elbow = old_vector(6);

                    const Waypoint current_waypoint = *waypoint_iterator;
                    waypoint_has_elbow = current_waypoint.elbow.has_value();
                    auto target_position_vector = current_waypoint.getTargetVector(Affine(), old_affine, old_elbow);

                    setVector(input_parameters->SelectionVector, VectorCartRotElbow(true, true, waypoint_has_elbow));
                    setInputLimits(input_parameters.get(), current_waypoint, data);
                    setVector(input_parameters->TargetPositionVector, target_position_vector);
                    setVector(input_parameters->TargetVelocityVector, current_waypoint.velocity);

                    old_affine = current_waypoint.getTargetAffine(Affine(), old_affine);
                    old_vector = target_position_vector;
                    old_elbow = old_vector(6);
                } else {
                    return franka::MotionFinished(CartesianPose(input_parameters->CurrentPositionVector, waypoint_has_elbow));
                }
            }
        }

#ifdef WITH_PYTHON
        if (PyErr_CheckSignals() == -1) {
            stop();
        }
#endif

        const int steps = std::max<int>(period.toMSec(), 1);
        for (int i = 0; i < steps; i++) {
            result_value = rml->RMLPosition(*input_parameters, output_parameters.get(), flags);

            if (current_motion.reload || result_value == ReflexxesAPI::RML_FINAL_STATE_REACHED) {
                waypoint_iterator += 1;

                if (current_motion.reload) {
                    waypoint_iterator = current_motion.waypoints.begin();
                    current_motion.reload = false;

                } else if (waypoint_iterator == current_motion.waypoints.end()) {
                    return franka::MotionFinished(CartesianPose(input_parameters->CurrentPositionVector, waypoint_has_elbow));
                }

                const Waypoint current_waypoint = *waypoint_iterator;
                waypoint_has_elbow = current_waypoint.elbow.has_value();
                auto target_position_vector = current_waypoint.getTargetVector(frame, old_affine, old_elbow);

                setVector(input_parameters->SelectionVector, VectorCartRotElbow(true, true, waypoint_has_elbow));
                setInputLimits(input_parameters.get(), current_waypoint, data);
                setVector(input_parameters->TargetPositionVector, target_position_vector);
                setVector(input_parameters->TargetVelocityVector, current_waypoint.velocity);

                old_affine = current_waypoint.getTargetAffine(frame, old_affine);
                old_vector = target_position_vector;
                old_elbow = old_vector(6);

            } else if (result_value == ReflexxesAPI::RML_ERROR_INVALID_INPUT_VALUES) {
                std::cout << "Invalid inputs:" << std::endl;
                return franka::MotionFinished(CartesianPose(input_parameters->CurrentPositionVector, waypoint_has_elbow));
            }

            *input_parameters->CurrentPositionVector = *output_parameters->NewPositionVector;
            *input_parameters->CurrentVelocityVector = *output_parameters->NewVelocityVector;
            *input_parameters->CurrentAccelerationVector = *output_parameters->NewAccelerationVector;
        }

        return CartesianPose(output_parameters->NewPositionVector, waypoint_has_elbow);
    };

    try {
        control(motion_generator, controller_mode);

    } catch (franka::Exception exception) {
        std::cout << exception.what() << std::endl;
        return false;
    }
    return true;
}

void Robot::setInputLimits(RMLPositionInputParameters *input_parameters, const MotionData& data) {
    setInputLimits(input_parameters, Waypoint(), data);
}

void Robot::setInputLimits(RMLPositionInputParameters *input_parameters, const Waypoint& waypoint, const MotionData& data) {
    constexpr double translation_factor {0.5};
    constexpr double elbow_factor {0.38};
    constexpr double derivative_factor {0.4};

    setVector(input_parameters->MaxVelocityVector, VectorCartRotElbow(
        translation_factor * waypoint.velocity_rel * data.velocity_rel * velocity_rel * max_translation_velocity,
        waypoint.velocity_rel * data.velocity_rel * velocity_rel * max_rotation_velocity,
        elbow_factor * waypoint.velocity_rel * data.velocity_rel * velocity_rel * max_elbow_velocity
    ));
    setVector(input_parameters->MaxAccelerationVector, VectorCartRotElbow(
        translation_factor * derivative_factor * data.acceleration_rel * acceleration_rel * max_translation_acceleration,
        derivative_factor * data.acceleration_rel * acceleration_rel * max_rotation_acceleration,
        elbow_factor * derivative_factor * data.acceleration_rel * acceleration_rel * max_elbow_acceleration
    ));
    setVector(input_parameters->MaxJerkVector, VectorCartRotElbow(
        translation_factor * std::pow(derivative_factor, 2) * data.jerk_rel * jerk_rel * max_translation_jerk,
        std::pow(derivative_factor, 2) * data.jerk_rel * jerk_rel * max_rotation_jerk,
        elbow_factor * std::pow(derivative_factor, 2) * data.jerk_rel * jerk_rel * max_elbow_jerk
    ));

    if (waypoint.minimum_time.has_value()) {
        input_parameters->SetMinimumSynchronizationTime(waypoint.minimum_time.value());
    }
}

} // namepace frankx
