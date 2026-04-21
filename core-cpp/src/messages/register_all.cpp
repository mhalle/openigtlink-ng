// GENERATED — do not edit. Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/register_all.hpp"

#include "oigtl/messages/bind.hpp"
#include "oigtl/messages/capability.hpp"
#include "oigtl/messages/colort.hpp"
#include "oigtl/messages/colortable.hpp"
#include "oigtl/messages/command.hpp"
#include "oigtl/messages/ext_header.hpp"
#include "oigtl/messages/get_bind.hpp"
#include "oigtl/messages/get_capabil.hpp"
#include "oigtl/messages/get_colort.hpp"
#include "oigtl/messages/get_image.hpp"
#include "oigtl/messages/get_imgmeta.hpp"
#include "oigtl/messages/get_lbmeta.hpp"
#include "oigtl/messages/get_ndarray.hpp"
#include "oigtl/messages/get_point.hpp"
#include "oigtl/messages/get_polydata.hpp"
#include "oigtl/messages/get_position.hpp"
#include "oigtl/messages/get_qtdata.hpp"
#include "oigtl/messages/get_qtrans.hpp"
#include "oigtl/messages/get_sensor.hpp"
#include "oigtl/messages/get_status.hpp"
#include "oigtl/messages/get_string.hpp"
#include "oigtl/messages/get_tdata.hpp"
#include "oigtl/messages/get_traj.hpp"
#include "oigtl/messages/get_trans.hpp"
#include "oigtl/messages/get_vmeta.hpp"
#include "oigtl/messages/header.hpp"
#include "oigtl/messages/image.hpp"
#include "oigtl/messages/imgmeta.hpp"
#include "oigtl/messages/lbmeta.hpp"
#include "oigtl/messages/metadata.hpp"
#include "oigtl/messages/ndarray.hpp"
#include "oigtl/messages/point.hpp"
#include "oigtl/messages/polydata.hpp"
#include "oigtl/messages/position.hpp"
#include "oigtl/messages/qtdata.hpp"
#include "oigtl/messages/qtrans.hpp"
#include "oigtl/messages/rts_bind.hpp"
#include "oigtl/messages/rts_capabil.hpp"
#include "oigtl/messages/rts_command.hpp"
#include "oigtl/messages/rts_image.hpp"
#include "oigtl/messages/rts_imgmeta.hpp"
#include "oigtl/messages/rts_lbmeta.hpp"
#include "oigtl/messages/rts_ndarray.hpp"
#include "oigtl/messages/rts_point.hpp"
#include "oigtl/messages/rts_polydata.hpp"
#include "oigtl/messages/rts_position.hpp"
#include "oigtl/messages/rts_qtdata.hpp"
#include "oigtl/messages/rts_qtrans.hpp"
#include "oigtl/messages/rts_sensor.hpp"
#include "oigtl/messages/rts_status.hpp"
#include "oigtl/messages/rts_string.hpp"
#include "oigtl/messages/rts_tdata.hpp"
#include "oigtl/messages/rts_traj.hpp"
#include "oigtl/messages/rts_trans.hpp"
#include "oigtl/messages/sensor.hpp"
#include "oigtl/messages/status.hpp"
#include "oigtl/messages/stp_bind.hpp"
#include "oigtl/messages/stp_image.hpp"
#include "oigtl/messages/stp_ndarray.hpp"
#include "oigtl/messages/stp_polydata.hpp"
#include "oigtl/messages/stp_position.hpp"
#include "oigtl/messages/stp_qtdata.hpp"
#include "oigtl/messages/stp_qtrans.hpp"
#include "oigtl/messages/stp_sensor.hpp"
#include "oigtl/messages/stp_tdata.hpp"
#include "oigtl/messages/stp_trans.hpp"
#include "oigtl/messages/stp_video.hpp"
#include "oigtl/messages/string.hpp"
#include "oigtl/messages/stt_bind.hpp"
#include "oigtl/messages/stt_image.hpp"
#include "oigtl/messages/stt_ndarray.hpp"
#include "oigtl/messages/stt_polydata.hpp"
#include "oigtl/messages/stt_position.hpp"
#include "oigtl/messages/stt_qtdata.hpp"
#include "oigtl/messages/stt_qtrans.hpp"
#include "oigtl/messages/stt_tdata.hpp"
#include "oigtl/messages/stt_trans.hpp"
#include "oigtl/messages/stt_video.hpp"
#include "oigtl/messages/tdata.hpp"
#include "oigtl/messages/traj.hpp"
#include "oigtl/messages/transform.hpp"
#include "oigtl/messages/unit.hpp"
#include "oigtl/messages/video.hpp"
#include "oigtl/messages/videometa.hpp"

namespace oigtl::messages {

void register_all(oigtl::runtime::Registry& registry) {
    registry.register_message_type(
        "BIND",
        [](const std::uint8_t* data, std::size_t length) {
            return Bind::unpack(data, length).pack();
        });
    registry.register_message_type(
        "CAPABILITY",
        [](const std::uint8_t* data, std::size_t length) {
            return Capability::unpack(data, length).pack();
        });
    registry.register_message_type(
        "COLORT",
        [](const std::uint8_t* data, std::size_t length) {
            return Colort::unpack(data, length).pack();
        });
    registry.register_message_type(
        "COLORTABLE",
        [](const std::uint8_t* data, std::size_t length) {
            return Colortable::unpack(data, length).pack();
        });
    registry.register_message_type(
        "COMMAND",
        [](const std::uint8_t* data, std::size_t length) {
            return Command::unpack(data, length).pack();
        });
    registry.register_message_type(
        "EXT_HEADER",
        [](const std::uint8_t* data, std::size_t length) {
            return ExtHeader::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_BIND",
        [](const std::uint8_t* data, std::size_t length) {
            return GetBind::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_CAPABIL",
        [](const std::uint8_t* data, std::size_t length) {
            return GetCapabil::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_COLORT",
        [](const std::uint8_t* data, std::size_t length) {
            return GetColort::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_IMAGE",
        [](const std::uint8_t* data, std::size_t length) {
            return GetImage::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_IMGMETA",
        [](const std::uint8_t* data, std::size_t length) {
            return GetImgmeta::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_LBMETA",
        [](const std::uint8_t* data, std::size_t length) {
            return GetLbmeta::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_NDARRAY",
        [](const std::uint8_t* data, std::size_t length) {
            return GetNdarray::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_POINT",
        [](const std::uint8_t* data, std::size_t length) {
            return GetPoint::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_POLYDATA",
        [](const std::uint8_t* data, std::size_t length) {
            return GetPolydata::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_POSITION",
        [](const std::uint8_t* data, std::size_t length) {
            return GetPosition::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_QTDATA",
        [](const std::uint8_t* data, std::size_t length) {
            return GetQtdata::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_QTRANS",
        [](const std::uint8_t* data, std::size_t length) {
            return GetQtrans::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_SENSOR",
        [](const std::uint8_t* data, std::size_t length) {
            return GetSensor::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_STATUS",
        [](const std::uint8_t* data, std::size_t length) {
            return GetStatus::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_STRING",
        [](const std::uint8_t* data, std::size_t length) {
            return GetString::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_TDATA",
        [](const std::uint8_t* data, std::size_t length) {
            return GetTdata::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_TRAJ",
        [](const std::uint8_t* data, std::size_t length) {
            return GetTraj::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_TRANS",
        [](const std::uint8_t* data, std::size_t length) {
            return GetTrans::unpack(data, length).pack();
        });
    registry.register_message_type(
        "GET_VMETA",
        [](const std::uint8_t* data, std::size_t length) {
            return GetVmeta::unpack(data, length).pack();
        });
    registry.register_message_type(
        "HEADER",
        [](const std::uint8_t* data, std::size_t length) {
            return Header::unpack(data, length).pack();
        });
    registry.register_message_type(
        "IMAGE",
        [](const std::uint8_t* data, std::size_t length) {
            return Image::unpack(data, length).pack();
        });
    registry.register_message_type(
        "IMGMETA",
        [](const std::uint8_t* data, std::size_t length) {
            return Imgmeta::unpack(data, length).pack();
        });
    registry.register_message_type(
        "LBMETA",
        [](const std::uint8_t* data, std::size_t length) {
            return Lbmeta::unpack(data, length).pack();
        });
    registry.register_message_type(
        "METADATA",
        [](const std::uint8_t* data, std::size_t length) {
            return Metadata::unpack(data, length).pack();
        });
    registry.register_message_type(
        "NDARRAY",
        [](const std::uint8_t* data, std::size_t length) {
            return Ndarray::unpack(data, length).pack();
        });
    registry.register_message_type(
        "POINT",
        [](const std::uint8_t* data, std::size_t length) {
            return Point::unpack(data, length).pack();
        });
    registry.register_message_type(
        "POLYDATA",
        [](const std::uint8_t* data, std::size_t length) {
            return Polydata::unpack(data, length).pack();
        });
    registry.register_message_type(
        "POSITION",
        [](const std::uint8_t* data, std::size_t length) {
            return Position::unpack(data, length).pack();
        });
    registry.register_message_type(
        "QTDATA",
        [](const std::uint8_t* data, std::size_t length) {
            return Qtdata::unpack(data, length).pack();
        });
    registry.register_message_type(
        "QTRANS",
        [](const std::uint8_t* data, std::size_t length) {
            return Qtrans::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_BIND",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsBind::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_CAPABIL",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsCapabil::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_COMMAND",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsCommand::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_IMAGE",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsImage::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_IMGMETA",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsImgmeta::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_LBMETA",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsLbmeta::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_NDARRAY",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsNdarray::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_POINT",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsPoint::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_POLYDATA",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsPolydata::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_POSITION",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsPosition::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_QTDATA",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsQtdata::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_QTRANS",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsQtrans::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_SENSOR",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsSensor::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_STATUS",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsStatus::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_STRING",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsString::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_TDATA",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsTdata::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_TRAJ",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsTraj::unpack(data, length).pack();
        });
    registry.register_message_type(
        "RTS_TRANS",
        [](const std::uint8_t* data, std::size_t length) {
            return RtsTrans::unpack(data, length).pack();
        });
    registry.register_message_type(
        "SENSOR",
        [](const std::uint8_t* data, std::size_t length) {
            return Sensor::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STATUS",
        [](const std::uint8_t* data, std::size_t length) {
            return Status::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STP_BIND",
        [](const std::uint8_t* data, std::size_t length) {
            return StpBind::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STP_IMAGE",
        [](const std::uint8_t* data, std::size_t length) {
            return StpImage::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STP_NDARRAY",
        [](const std::uint8_t* data, std::size_t length) {
            return StpNdarray::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STP_POLYDATA",
        [](const std::uint8_t* data, std::size_t length) {
            return StpPolydata::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STP_POSITION",
        [](const std::uint8_t* data, std::size_t length) {
            return StpPosition::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STP_QTDATA",
        [](const std::uint8_t* data, std::size_t length) {
            return StpQtdata::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STP_QTRANS",
        [](const std::uint8_t* data, std::size_t length) {
            return StpQtrans::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STP_SENSOR",
        [](const std::uint8_t* data, std::size_t length) {
            return StpSensor::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STP_TDATA",
        [](const std::uint8_t* data, std::size_t length) {
            return StpTdata::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STP_TRANS",
        [](const std::uint8_t* data, std::size_t length) {
            return StpTrans::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STP_VIDEO",
        [](const std::uint8_t* data, std::size_t length) {
            return StpVideo::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STRING",
        [](const std::uint8_t* data, std::size_t length) {
            return String::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STT_BIND",
        [](const std::uint8_t* data, std::size_t length) {
            return SttBind::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STT_IMAGE",
        [](const std::uint8_t* data, std::size_t length) {
            return SttImage::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STT_NDARRAY",
        [](const std::uint8_t* data, std::size_t length) {
            return SttNdarray::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STT_POLYDATA",
        [](const std::uint8_t* data, std::size_t length) {
            return SttPolydata::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STT_POSITION",
        [](const std::uint8_t* data, std::size_t length) {
            return SttPosition::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STT_QTDATA",
        [](const std::uint8_t* data, std::size_t length) {
            return SttQtdata::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STT_QTRANS",
        [](const std::uint8_t* data, std::size_t length) {
            return SttQtrans::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STT_TDATA",
        [](const std::uint8_t* data, std::size_t length) {
            return SttTdata::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STT_TRANS",
        [](const std::uint8_t* data, std::size_t length) {
            return SttTrans::unpack(data, length).pack();
        });
    registry.register_message_type(
        "STT_VIDEO",
        [](const std::uint8_t* data, std::size_t length) {
            return SttVideo::unpack(data, length).pack();
        });
    registry.register_message_type(
        "TDATA",
        [](const std::uint8_t* data, std::size_t length) {
            return Tdata::unpack(data, length).pack();
        });
    registry.register_message_type(
        "TRAJ",
        [](const std::uint8_t* data, std::size_t length) {
            return Traj::unpack(data, length).pack();
        });
    registry.register_message_type(
        "TRANSFORM",
        [](const std::uint8_t* data, std::size_t length) {
            return Transform::unpack(data, length).pack();
        });
    registry.register_message_type(
        "UNIT",
        [](const std::uint8_t* data, std::size_t length) {
            return Unit::unpack(data, length).pack();
        });
    registry.register_message_type(
        "VIDEO",
        [](const std::uint8_t* data, std::size_t length) {
            return Video::unpack(data, length).pack();
        });
    registry.register_message_type(
        "VIDEOMETA",
        [](const std::uint8_t* data, std::size_t length) {
            return Videometa::unpack(data, length).pack();
        });
}

oigtl::runtime::Registry default_registry() {
    oigtl::runtime::Registry registry;
    register_all(registry);
    return registry;
}

}  // namespace oigtl::messages
