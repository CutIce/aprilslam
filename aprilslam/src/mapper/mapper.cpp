#include "aprilslam/mapper.h"
#include "aprilslam/utils.h"
#include <gtsam/slam/PriorFactor.h>
#include <fstream>
#include <ros/ros.h>

namespace aprilslam
{

    using namespace gtsam;

    int Mapper::pose_cnt = 0;

    Mapper::Mapper(double relinearize_thresh, int relinearize_skip)
        : init_(false),
          params_(ISAM2GaussNewtonParams(), relinearize_thresh, relinearize_skip),
          isam2_(params_),
          tag_noise_(noiseModel::Diagonal::Sigmas((Vector(6) << 0.20, 0.20, 0.20, 0.10, 0.10, 0.10).finished())),
          small_noise_(noiseModel::Diagonal::Sigmas((Vector(6) << 0.02, 0.02, 0.02, 0.05, 0.05, 0.05).finished()))
    {
        lm_params_.setMaxIterations(10);
        // lm_params_.setVerbosity("ERROR");
    }

    void Mapper::UpdateTagsPriorInfo(const std::map<size_t, geometry_msgs::Pose> tag_prior_poses)
    {
        tag_prior_poses_ = tag_prior_poses;
    }

    void Mapper::AddPose(const geometry_msgs::Pose &pose)
    {
        pose_cnt++;

        // Transform geometey_msgs::pose to gtsam::Pose3
        pose_ = FromGeometryPose(pose);
        initial_estimates_.insert(Symbol('x', pose_cnt), pose_);
    }

    void Mapper::Initialize(const Apriltag &tag_w)
    {
        ROS_ASSERT_MSG(pose_cnt == 1, "Incorrect initial pose");
        
        //! ERROR HERE
        if(tag_prior_poses_.empty())
        {
            AddLandmark(tag_w, Pose3());
        }
        else
        {
            AddLandmark(tag_w, )
        }

        // A very strong prior on first pose
        ROS_INFO("Add pose prior on: %d", pose_cnt);
        graph_.push_back(PriorFactor<Pose3>(Symbol('x', pose_cnt), pose_, small_noise_));


        init_ = true;
    }

    void Mapper::AddLandmark(const Apriltag &tag_c, const Pose3 &pose)
    {
        initial_estimates_.insert(Symbol('l', tag_c.id), pose);
        all_ids_.insert(tag_c.id);
        all_tags_c_[tag_c.id] = tag_c;
        //AddLandmarkPrior(tag_c.id);
    }

    void Mapper::AddLandmarks(const std::vector<Apriltag> &tags_c)
    {
        for (const Apriltag &tag_c : tags_c)
        {
            // Only add landmark if it's not already added
            if (all_ids_.find(tag_c.id) == all_ids_.end())
            {
                const Pose3 &w_T_c = pose_;
                const Pose3 c_T_t = FromGeometryPose(tag_c.pose);
                const Pose3 w_T_t = w_T_c.compose(c_T_t);
                AddLandmark(tag_c, w_T_t);
            }
        }
    }

    void Mapper::AddLandmarkPrior(const size_t tag_id)
    {
        if (tag_prior_poses_.find(tag_id) == tag_prior_poses_.end())
        {
            ROS_WARN("There is no prior information about tag %d", tag_id);
            return;
        }
        else
        {
            ROS_INFO("Add landmark prior on: %d", tag_id);
            gtsam::Pose3 tag_prior_pose = FromGeometryPose(tag_prior_poses_.find(tag_id)->second);
            graph_.push_back(PriorFactor<Pose3>(Symbol('l', tag_id), tag_prior_pose, small_noise_));
            return;
        }
    }

    void Mapper::AddFactors(const std::vector<Apriltag> &tags_c)
    {
        Symbol x_i('x', pose_cnt);
        for (const Apriltag &tag_c : tags_c)
        {
            graph_.push_back(BetweenFactor<Pose3>(
                x_i, Symbol('l', tag_c.id), FromGeometryPose(tag_c.pose), tag_noise_));
        }
    }

    void Mapper::Optimize(int num_iterations)
    {
        isam2_.update(graph_, initial_estimates_);
        if (num_iterations > 1)
        {
            for (int i = 1; i < num_iterations; ++i)
            {
                isam2_.update();
            }
        }
    }

    void Mapper::BatchOptimize()
    {
        gtsam::LevenbergMarquardtOptimizer lm_optimizer(graph_, initial_estimates_, lm_params_);
        batch_results_ = lm_optimizer.optimize();
        std::ofstream graph_ofs("/home/wushaoteng/project/electroMechanical/catkin_ws/data/aprilslam.dot");
        graph_.saveGraph(graph_ofs, batch_results_);
    }

    void Mapper::BatchUpdate(TagMap *map, geometry_msgs::Pose *pose) const
    {
        ROS_ASSERT_MSG(all_ids_.size() == all_tags_c_.size(), "id and size mismatch");
        Values results = batch_results_;
        // Update the current pose
        const Pose3 &cam_pose = results.at<Pose3>(Symbol('x', pose_cnt));
        SetPosition(&pose->position, cam_pose.x(), cam_pose.y(), cam_pose.z());
        SetOrientation(&pose->orientation, cam_pose.rotation().toQuaternion());
        // Update the current map
        for (const int tag_id : all_ids_)
        {
            const Pose3 &tag_pose3 = results.at<Pose3>(Symbol('l', tag_id));
            geometry_msgs::Pose tag_pose;
            SetPosition(&tag_pose.position, tag_pose3.x(), tag_pose3.y(),
                        tag_pose3.z());
            SetOrientation(&tag_pose.orientation, tag_pose3.rotation().toQuaternion());
            // This should not change the size of all_sizes_ because all_sizes_ and
            // all_ids_ should have the same size
            auto it = all_tags_c_.find(tag_id);
            map->AddOrUpdate(it->second, tag_pose);
        }
    }

    void Mapper::Update(TagMap *map, geometry_msgs::Pose *pose) const
    {
        ROS_ASSERT_MSG(all_ids_.size() == all_tags_c_.size(), "id and size mismatch");
        Values results = isam2_.calculateEstimate();
        // Update the current pose
        const Pose3 &cam_pose = results.at<Pose3>(Symbol('x', pose_cnt));
        SetPosition(&pose->position, cam_pose.x(), cam_pose.y(), cam_pose.z());
        SetOrientation(&pose->orientation, cam_pose.rotation().toQuaternion());
        // Update the current map
        for (const int tag_id : all_ids_)
        {
            const Pose3 &tag_pose3 = results.at<Pose3>(Symbol('l', tag_id));
            geometry_msgs::Pose tag_pose;
            SetPosition(&tag_pose.position, tag_pose3.x(), tag_pose3.y(),
                        tag_pose3.z());
            SetOrientation(&tag_pose.orientation, tag_pose3.rotation().toQuaternion());
            // This should not change the size of all_sizes_ because all_sizes_ and
            // all_ids_ should have the same size
            auto it = all_tags_c_.find(tag_id);
            map->AddOrUpdate(it->second, tag_pose);
        }
    }

    void Mapper::Clear()
    {
        graph_.resize(0);
        initial_estimates_.clear();
    }

    Pose3 FromGeometryPose(const geometry_msgs::Pose &pose)
    {
        Point3 t(pose.position.x, pose.position.y, pose.position.z);
        Rot3 R(Eigen::Quaterniond(pose.orientation.w, pose.orientation.x,
                                  pose.orientation.y, pose.orientation.z));
        return Pose3(R, t);
    }

} // namespace aprilslam
