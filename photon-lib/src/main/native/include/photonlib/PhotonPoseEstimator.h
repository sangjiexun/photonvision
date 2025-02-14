/*
 * MIT License
 *
 * Copyright (c) 2022 PhotonVision
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include <frc/apriltag/AprilTagFieldLayout.h>
#include <frc/geometry/Pose3d.h>
#include <frc/geometry/Transform3d.h>

#include "photonlib/PhotonCamera.h"

namespace photonlib {
enum PoseStrategy : int {
  LOWEST_AMBIGUITY,
  CLOSEST_TO_CAMERA_HEIGHT,
  CLOSEST_TO_REFERENCE_POSE,
  CLOSEST_TO_LAST_POSE,
  AVERAGE_BEST_TARGETS
};

struct EstimatedRobotPose {
  /** The estimated pose */
  frc::Pose3d estimatedPose;
  /** The estimated time the frame used to derive the robot pose was taken, in
   * the same timebase as the RoboRIO FPGA Timestamp */
  units::second_t timestamp;

  EstimatedRobotPose(frc::Pose3d pose_, units::second_t time_)
      : estimatedPose(pose_), timestamp(time_) {}
};

/**
 * The PhotonPoseEstimator class filters or combines readings from all the
 * fiducials visible at a given timestamp on the field to produce a single robot
 * in field pose, using the strategy set below. Example usage can be found in
 * our apriltagExample example project.
 */
class PhotonPoseEstimator {
 public:
  using map_value_type =
      std::pair<std::shared_ptr<PhotonCamera>, frc::Transform3d>;
  using size_type = std::vector<map_value_type>::size_type;

  /**
   * Create a new PhotonPoseEstimator.
   *
   * <p>Example: {@code <code> <p> Map<Integer, Pose3d> map = new HashMap<>();
   * <p> map.put(1, new Pose3d(1.0, 2.0, 3.0, new Rotation3d())); // Tag ID 1 is
   * at (1.0,2.0,3.0) </code> }
   *
   * @param aprilTags A AprilTagFieldLayout linking AprilTag IDs to Pose3ds with
   * respect to the FIRST field.
   * @param strategy The strategy it should use to determine the best pose.
   * @param camera PhotonCameras and
   * @param robotToCamera Transform3d from the center of the robot to the camera
   * mount positions (ie, robot ➔ camera).
   */
  explicit PhotonPoseEstimator(frc::AprilTagFieldLayout aprilTags,
                               PoseStrategy strategy, PhotonCamera&& camera,
                               frc::Transform3d robotToCamera);

  /**
   * Get the AprilTagFieldLayout being used by the PositionEstimator.
   *
   * @return the AprilTagFieldLayout
   */
  frc::AprilTagFieldLayout GetFieldLayout() const { return aprilTags; }

  /**
   * Get the Position Estimation Strategy being used by the Position Estimator.
   *
   * @return the strategy
   */
  PoseStrategy GetPoseStrategy() const { return strategy; }

  /**
   * Set the Position Estimation Strategy used by the Position Estimator.
   *
   * @param strategy the strategy to set
   */
  inline void SetPoseStrategy(PoseStrategy strat) { strategy = strat; }

  /**
   * Return the reference position that is being used by the estimator.
   *
   * @return the referencePose
   */
  frc::Pose3d GetReferencePose() const { return referencePose; }

  /**
   * Update the stored reference pose for use when using the
   * CLOSEST_TO_REFERENCE_POSE strategy.
   *
   * @param referencePose the referencePose to set
   */
  inline void SetReferencePose(frc::Pose3d referencePose) {
    this->referencePose = referencePose;
  }

  /**
   * @return The current transform from the center of the robot to the camera
   *         mount position.
   */
  inline frc::Transform3d GetRobotToCameraTransform() {
    return m_robotToCamera;
  }

  /**
   * Useful for pan and tilt mechanisms, or cameras on turrets
   *
   * @param robotToCamera The current transform from the center of the robot to
   * the camera mount position.
   */
  inline void SetRobotToCameraTransform(frc::Transform3d robotToCamera) {
    m_robotToCamera = robotToCamera;
  }

  /**
   * Update the stored last pose. Useful for setting the initial estimate when
   * using the CLOSEST_TO_LAST_POSE strategy.
   *
   * @param lastPose the lastPose to set
   */
  inline void SetLastPose(frc::Pose3d lastPose) { this->lastPose = lastPose; }

  /**
   * Update the pose estimator. Internally grabs a new PhotonPipelineResult from
   * the camera and process it.
   */
  std::optional<EstimatedRobotPose> Update();

  /**
   * Update the pose estimator.
   */
  std::optional<EstimatedRobotPose> Update(const PhotonPipelineResult& result);

  inline PhotonCamera& GetCamera() { return camera; }

 private:
  frc::AprilTagFieldLayout aprilTags;
  PoseStrategy strategy;

  PhotonCamera camera;
  frc::Transform3d m_robotToCamera;

  frc::Pose3d lastPose;
  frc::Pose3d referencePose;

  /**
   * Return the estimated position of the robot with the lowest position
   * ambiguity from a List of pipeline results.
   *
   * @return the estimated position of the robot in the FCS and the estimated
   * timestamp of this estimation.
   */
  std::optional<EstimatedRobotPose> LowestAmbiguityStrategy(
      PhotonPipelineResult result);

  /**
   * Return the estimated position of the robot using the target with the lowest
   * delta height difference between the estimated and actual height of the
   * camera.
   *
   * @return the estimated position of the robot in the FCS and the estimated
   * timestamp of this estimation.
   */
  std::optional<EstimatedRobotPose> ClosestToCameraHeightStrategy(
      PhotonPipelineResult result);

  /**
   * Return the estimated position of the robot using the target with the lowest
   * delta in the vector magnitude between it and the reference pose.
   *
   * @param referencePose reference pose to check vector magnitude difference
   * against.
   * @return the estimated position of the robot in the FCS and the estimated
   * timestamp of this estimation.
   */
  std::optional<EstimatedRobotPose> ClosestToReferencePoseStrategy(
      PhotonPipelineResult result);

  /**
   * Return the average of the best target poses using ambiguity as weight.

   * @return the estimated position of the robot in the FCS and the estimated
   timestamp of this
   *     estimation.
   */
  std::optional<EstimatedRobotPose> AverageBestTargetsStrategy(
      PhotonPipelineResult result);
};

}  // namespace photonlib
