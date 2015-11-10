/** @file
    @brief Implementation

    @date 2015

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

// Copyright 2015 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Internal Includes
#include "VideoIMUFusionDevice.h"
#include <osvr/AnalysisPluginKit/AnalysisPluginKitC.h>
#include <osvr/ClientKit/InterfaceCallbackC.h>
#include <osvr/ClientKit/InterfaceStateC.h>
#include <osvr/Util/EigenInterop.h>
#include <osvr/Util/Verbosity.h>

// Generated JSON header file
#include "org_osvr_filter_videoimufusion_json.h"

// Library/third-party includes
// - none

// Standard includes
#include <iostream>
#include <cmath>

static const OSVR_ChannelCount FUSED_SENSOR_ID = 0;
static const OSVR_ChannelCount TRANSFORMED_VIDEO_SENSOR_ID = 1;

VideoIMUFusionDevice::VideoIMUFusionDevice(OSVR_PluginRegContext ctx,
                                           std::string const &name,
                                           std::string const &imuPath,
                                           std::string const &videoPath,
                                           VideoIMUFusionParams const &params)
    : m_fusion(params) {
    /// Create the initialization options
    OSVR_DeviceInitOptions opts = osvrDeviceCreateInitOptions(ctx);

    osvrDeviceTrackerConfigure(opts, &m_trackerOut);

    /// Create the device token with the options
    OSVR_DeviceToken dev;
    if (OSVR_RETURN_FAILURE ==
        osvrAnalysisSyncInit(ctx, name.c_str(), opts, &dev, &m_clientCtx)) {
        throw std::runtime_error("Could not initialize analysis plugin!");
    }
    m_dev = osvr::pluginkit::DeviceToken(dev);

    /// Send JSON descriptor
    m_dev.sendJsonDescriptor(org_osvr_filter_videoimufusion_json);

    /// Register update callback
    m_dev.registerUpdateCallback(this);

    /// Set up to receive our input.
    osvrClientGetInterface(m_clientCtx, imuPath.c_str(), &m_imu);
    osvrRegisterOrientationCallback(
        m_imu, &VideoIMUFusionDevice::s_handleIMUData, this);
    osvrRegisterAngularVelocityCallback(
        m_imu, &VideoIMUFusionDevice::s_handleIMUVelocity, this);

    osvrClientGetInterface(m_clientCtx, videoPath.c_str(), &m_videoTracker);

    osvrRegisterPoseCallback(
        m_videoTracker, &VideoIMUFusionDevice::s_handleVideoTrackerData, this);
}

VideoIMUFusionDevice::~VideoIMUFusionDevice() {
    /// free the interfaces before the pointed-to function objects
    /// disappear.
    if (m_imu) {
        osvrClientFreeInterface(m_clientCtx, m_imu);
        m_imu = nullptr;
    }
    if (m_videoTracker) {
        osvrClientFreeInterface(m_clientCtx, m_videoTracker);
        m_imu = nullptr;
    }
}

void VideoIMUFusionDevice::s_handleIMUData(
    void *userdata, const OSVR_TimeValue *timestamp,
    const OSVR_OrientationReport *report) {
    static_cast<VideoIMUFusionDevice *>(userdata)
        ->handleIMUData(*timestamp, *report);
}
void VideoIMUFusionDevice::s_handleIMUVelocity(
    void *userdata, const OSVR_TimeValue *timestamp,
    const OSVR_AngularVelocityReport *report) {
    static_cast<VideoIMUFusionDevice *>(userdata)
        ->handleIMUVelocity(*timestamp, *report);
}
void VideoIMUFusionDevice::s_handleVideoTrackerData(
    void *userdata, const OSVR_TimeValue *timestamp,
    const OSVR_PoseReport *report) {
    static_cast<VideoIMUFusionDevice *>(userdata)
        ->handleVideoTrackerData(*timestamp, *report);
}

void VideoIMUFusionDevice::handleIMUData(const OSVR_TimeValue &timestamp,
                                         const OSVR_OrientationReport &report) {
    m_fusion.handleIMUData(timestamp, report);
    if (m_fusion.running()) {
        sendMainPoseReport();
    }
}
void VideoIMUFusionDevice::handleIMUVelocity(
    const OSVR_TimeValue &timestamp, const OSVR_AngularVelocityReport &report) {

    using namespace osvr::util::eigen_interop;
    Eigen::Quaterniond q = map(report.state.incrementalRotation);
    Eigen::Vector3d rot;
    if (q.w() >= 1.) {
        rot = Eigen::Vector3d::Zero();
    } else {
        auto magnitude = q.vec().blueNorm();
        rot = (q.vec() / magnitude * (2. * std::atan2(magnitude, q.w()))) /
              report.state.dt;
        /// @todo without transformations being applied to vel quats, this
        /// is needed.
        // std::swap(rot[0], rot[1]);
        rot[1] *= -1.;
        rot[2] *= -1.;
    }
#if 0
	static int s = 0;

	if (s == 0) {
        static const Eigen::IOFormat fmt(3, 0, ", ", "; ", ", ", "[", "]");
        OSVR_DEV_VERBOSE(rot.transpose().format(fmt));
	}

	s = (s + 1) % 100;
#endif

    m_fusion.handleIMUVelocity(timestamp, rot);
#if 0
    if (m_fusion.running()) {
        sendMainPoseReport();
    }
#endif
}
void VideoIMUFusionDevice::handleVideoTrackerData(
    const OSVR_TimeValue &timestamp, const OSVR_PoseReport &report) {
    if (!m_fusion.running()) {
        auto ts = OSVR_TimeValue{};
        auto oriState = OSVR_OrientationState{};
        auto ret = osvrGetOrientationState(m_imu, &ts, &oriState);
        if (ret != OSVR_RETURN_SUCCESS) {
            std::cout << "Got a video report before an IMU report, ignoring it"
                      << std::endl;
            return;
        }
        m_fusion.handleVideoTrackerDataDuringStartup(timestamp, report,
                                                     oriState);
        return;
    }
    m_fusion.handleVideoTrackerDataWhileRunning(timestamp, report);
    sendMainPoseReport();
    osvrDeviceTrackerSendPoseTimestamped(
        m_dev, m_trackerOut, &m_fusion.getLatestReorientedVideoPose(),
        TRANSFORMED_VIDEO_SENSOR_ID, &timestamp);
}

void VideoIMUFusionDevice::sendMainPoseReport() {
    osvrDeviceTrackerSendPoseTimestamped(
        m_dev, m_trackerOut, &m_fusion.getLatestFusedPose(), FUSED_SENSOR_ID,
        &m_fusion.getLatestFusedTime());
}
