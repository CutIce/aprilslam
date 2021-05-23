#ifndef APRILTAG_ROS_MAPPER_H_
#define APRILTAG_ROS_MAPPER_H_

#include <gtsam/inference/Symbol.h>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/nonlinear/ISAM2.h>
#include <gtsam/nonlinear/Values.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/LevenbergMarquardtParams.h>
#include <gtsam/slam/BetweenFactor.h>

#include <geometry_msgs/Pose.h>
#include <aprilslam/Apriltags.h>
#include "aprilslam/tag_map.h"

namespace aprilslam
{

    // Feel like using iSAM2?
    class Mapper
    {
    public:
        static int pose_cnt;

        Mapper(double relinearize_thresh, int relinearize_skip);

        bool init() const { return init_; }
        void Optimize(int num_iterations = 1);
        void Update(aprilslam::TagMap *map, geometry_msgs::Pose *pose) const;
        void AddPose(const geometry_msgs::Pose &pose);
        void AddFactors(const std::vector<aprilslam::Apriltag> &tags_c);
        void AddLandmarks(const std::vector<aprilslam::Apriltag> &tags_c);
        void AddLandmarkPrior(const size_t tag_id);
        void Initialize(const Apriltag &tag_w);
        void Clear();
        void UpdateTagsPriorInfo(const std::map<size_t, geometry_msgs::Pose> tag_prior_poses);

        void BatchOptimize();
        void BatchUpdate(aprilslam::TagMap *map, geometry_msgs::Pose *pose) const;

    private:
        void AddLandmark(const aprilslam::Apriltag &tag_w,
                         const gtsam::Pose3 &pose);

        bool init_;
        gtsam::ISAM2Params params_;
        gtsam::ISAM2 isam2_;
        gtsam::NonlinearFactorGraph graph_;
        gtsam::Values initial_estimates_;
        gtsam::Pose3 pose_;
        gtsam::noiseModel::Diagonal::shared_ptr tag_noise_;
        gtsam::noiseModel::Diagonal::shared_ptr small_noise_;
        std::set<int> all_ids_;
        std::map<int, aprilslam::Apriltag> all_tags_c_;
        std::map<size_t, geometry_msgs::Pose> tag_prior_poses_;

        gtsam::LevenbergMarquardtParams lm_params_;
        gtsam::Values batch_results_;
    };

    gtsam::Pose3 FromGeometryPose(const geometry_msgs::Pose &pose);
} // namespace aprilslam

#endif // APRILSLAM_MAPPER_H_
