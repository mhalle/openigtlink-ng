# GENERATED — do not edit. Regenerate with: uv run oigtl-corpus codegen python
"""Generated typed message classes — one per OpenIGTLink wire type_id.

Each class is a Pydantic model with:

- ``TYPE_ID``     : the wire type_id string (class variable)
- ``BODY_SIZE``   : the fixed body byte size (fixed-body messages only)
- typed Pydantic fields matching the schema's body
- ``pack(self) -> bytes`` : produce wire body bytes
- ``unpack(cls, data: bytes) -> Self`` : decode wire body bytes
"""

from __future__ import annotations

from oigtl.messages.bind import Bind
from oigtl.messages.capability import Capability
from oigtl.messages.colort import Colort
from oigtl.messages.colortable import Colortable
from oigtl.messages.command import Command
from oigtl.messages.ext_header import ExtHeader
from oigtl.messages.get_bind import GetBind
from oigtl.messages.get_capabil import GetCapabil
from oigtl.messages.get_colort import GetColort
from oigtl.messages.get_image import GetImage
from oigtl.messages.get_imgmeta import GetImgmeta
from oigtl.messages.get_lbmeta import GetLbmeta
from oigtl.messages.get_ndarray import GetNdarray
from oigtl.messages.get_point import GetPoint
from oigtl.messages.get_polydata import GetPolydata
from oigtl.messages.get_position import GetPosition
from oigtl.messages.get_qtdata import GetQtdata
from oigtl.messages.get_qtrans import GetQtrans
from oigtl.messages.get_sensor import GetSensor
from oigtl.messages.get_status import GetStatus
from oigtl.messages.get_string import GetString
from oigtl.messages.get_tdata import GetTdata
from oigtl.messages.get_traj import GetTraj
from oigtl.messages.get_trans import GetTrans
from oigtl.messages.get_vmeta import GetVmeta
from oigtl.messages.header import Header
from oigtl.messages.image import Image
from oigtl.messages.imgmeta import Imgmeta
from oigtl.messages.lbmeta import Lbmeta
from oigtl.messages.metadata import Metadata
from oigtl.messages.ndarray import Ndarray
from oigtl.messages.point import Point
from oigtl.messages.polydata import Polydata
from oigtl.messages.position import Position
from oigtl.messages.qtdata import Qtdata
from oigtl.messages.qtrans import Qtrans
from oigtl.messages.rts_bind import RtsBind
from oigtl.messages.rts_capabil import RtsCapabil
from oigtl.messages.rts_command import RtsCommand
from oigtl.messages.rts_image import RtsImage
from oigtl.messages.rts_imgmeta import RtsImgmeta
from oigtl.messages.rts_lbmeta import RtsLbmeta
from oigtl.messages.rts_ndarray import RtsNdarray
from oigtl.messages.rts_point import RtsPoint
from oigtl.messages.rts_polydata import RtsPolydata
from oigtl.messages.rts_position import RtsPosition
from oigtl.messages.rts_qtdata import RtsQtdata
from oigtl.messages.rts_qtrans import RtsQtrans
from oigtl.messages.rts_sensor import RtsSensor
from oigtl.messages.rts_status import RtsStatus
from oigtl.messages.rts_string import RtsString
from oigtl.messages.rts_tdata import RtsTdata
from oigtl.messages.rts_traj import RtsTraj
from oigtl.messages.rts_trans import RtsTrans
from oigtl.messages.sensor import Sensor
from oigtl.messages.status import Status
from oigtl.messages.stp_bind import StpBind
from oigtl.messages.stp_image import StpImage
from oigtl.messages.stp_ndarray import StpNdarray
from oigtl.messages.stp_polydata import StpPolydata
from oigtl.messages.stp_position import StpPosition
from oigtl.messages.stp_qtdata import StpQtdata
from oigtl.messages.stp_qtrans import StpQtrans
from oigtl.messages.stp_sensor import StpSensor
from oigtl.messages.stp_tdata import StpTdata
from oigtl.messages.stp_trans import StpTrans
from oigtl.messages.stp_video import StpVideo
from oigtl.messages.string import String
from oigtl.messages.stt_bind import SttBind
from oigtl.messages.stt_image import SttImage
from oigtl.messages.stt_ndarray import SttNdarray
from oigtl.messages.stt_polydata import SttPolydata
from oigtl.messages.stt_position import SttPosition
from oigtl.messages.stt_qtdata import SttQtdata
from oigtl.messages.stt_qtrans import SttQtrans
from oigtl.messages.stt_tdata import SttTdata
from oigtl.messages.stt_trans import SttTrans
from oigtl.messages.stt_video import SttVideo
from oigtl.messages.tdata import Tdata
from oigtl.messages.traj import Traj
from oigtl.messages.transform import Transform
from oigtl.messages.unit import Unit
from oigtl.messages.video import Video
from oigtl.messages.videometa import Videometa

# type_id → message class lookup. Populated at module load.
REGISTRY: dict[str, type] = {
    "BIND": Bind,
    "CAPABILITY": Capability,
    "COLORT": Colort,
    "COLORTABLE": Colortable,
    "COMMAND": Command,
    "EXT_HEADER": ExtHeader,
    "GET_BIND": GetBind,
    "GET_CAPABIL": GetCapabil,
    "GET_COLORT": GetColort,
    "GET_IMAGE": GetImage,
    "GET_IMGMETA": GetImgmeta,
    "GET_LBMETA": GetLbmeta,
    "GET_NDARRAY": GetNdarray,
    "GET_POINT": GetPoint,
    "GET_POLYDATA": GetPolydata,
    "GET_POSITION": GetPosition,
    "GET_QTDATA": GetQtdata,
    "GET_QTRANS": GetQtrans,
    "GET_SENSOR": GetSensor,
    "GET_STATUS": GetStatus,
    "GET_STRING": GetString,
    "GET_TDATA": GetTdata,
    "GET_TRAJ": GetTraj,
    "GET_TRANS": GetTrans,
    "GET_VMETA": GetVmeta,
    "HEADER": Header,
    "IMAGE": Image,
    "IMGMETA": Imgmeta,
    "LBMETA": Lbmeta,
    "METADATA": Metadata,
    "NDARRAY": Ndarray,
    "POINT": Point,
    "POLYDATA": Polydata,
    "POSITION": Position,
    "QTDATA": Qtdata,
    "QTRANS": Qtrans,
    "RTS_BIND": RtsBind,
    "RTS_CAPABIL": RtsCapabil,
    "RTS_COMMAND": RtsCommand,
    "RTS_IMAGE": RtsImage,
    "RTS_IMGMETA": RtsImgmeta,
    "RTS_LBMETA": RtsLbmeta,
    "RTS_NDARRAY": RtsNdarray,
    "RTS_POINT": RtsPoint,
    "RTS_POLYDATA": RtsPolydata,
    "RTS_POSITION": RtsPosition,
    "RTS_QTDATA": RtsQtdata,
    "RTS_QTRANS": RtsQtrans,
    "RTS_SENSOR": RtsSensor,
    "RTS_STATUS": RtsStatus,
    "RTS_STRING": RtsString,
    "RTS_TDATA": RtsTdata,
    "RTS_TRAJ": RtsTraj,
    "RTS_TRANS": RtsTrans,
    "SENSOR": Sensor,
    "STATUS": Status,
    "STP_BIND": StpBind,
    "STP_IMAGE": StpImage,
    "STP_NDARRAY": StpNdarray,
    "STP_POLYDATA": StpPolydata,
    "STP_POSITION": StpPosition,
    "STP_QTDATA": StpQtdata,
    "STP_QTRANS": StpQtrans,
    "STP_SENSOR": StpSensor,
    "STP_TDATA": StpTdata,
    "STP_TRANS": StpTrans,
    "STP_VIDEO": StpVideo,
    "STRING": String,
    "STT_BIND": SttBind,
    "STT_IMAGE": SttImage,
    "STT_NDARRAY": SttNdarray,
    "STT_POLYDATA": SttPolydata,
    "STT_POSITION": SttPosition,
    "STT_QTDATA": SttQtdata,
    "STT_QTRANS": SttQtrans,
    "STT_TDATA": SttTdata,
    "STT_TRANS": SttTrans,
    "STT_VIDEO": SttVideo,
    "TDATA": Tdata,
    "TRAJ": Traj,
    "TRANSFORM": Transform,
    "UNIT": Unit,
    "VIDEO": Video,
    "VIDEOMETA": Videometa,
}


def default_registry() -> dict[str, type]:
    """Return a dict mapping wire type_id → message class."""
    return dict(REGISTRY)


# Re-exported high-level helpers (hand-written, lives next door).
from oigtl.messages._dispatch import extract_content_bytes, parse_message


__all__ = [
    "REGISTRY",
    "default_registry",
    "parse_message",
    "extract_content_bytes",
    "Bind",
    "Capability",
    "Colort",
    "Colortable",
    "Command",
    "ExtHeader",
    "GetBind",
    "GetCapabil",
    "GetColort",
    "GetImage",
    "GetImgmeta",
    "GetLbmeta",
    "GetNdarray",
    "GetPoint",
    "GetPolydata",
    "GetPosition",
    "GetQtdata",
    "GetQtrans",
    "GetSensor",
    "GetStatus",
    "GetString",
    "GetTdata",
    "GetTraj",
    "GetTrans",
    "GetVmeta",
    "Header",
    "Image",
    "Imgmeta",
    "Lbmeta",
    "Metadata",
    "Ndarray",
    "Point",
    "Polydata",
    "Position",
    "Qtdata",
    "Qtrans",
    "RtsBind",
    "RtsCapabil",
    "RtsCommand",
    "RtsImage",
    "RtsImgmeta",
    "RtsLbmeta",
    "RtsNdarray",
    "RtsPoint",
    "RtsPolydata",
    "RtsPosition",
    "RtsQtdata",
    "RtsQtrans",
    "RtsSensor",
    "RtsStatus",
    "RtsString",
    "RtsTdata",
    "RtsTraj",
    "RtsTrans",
    "Sensor",
    "Status",
    "StpBind",
    "StpImage",
    "StpNdarray",
    "StpPolydata",
    "StpPosition",
    "StpQtdata",
    "StpQtrans",
    "StpSensor",
    "StpTdata",
    "StpTrans",
    "StpVideo",
    "String",
    "SttBind",
    "SttImage",
    "SttNdarray",
    "SttPolydata",
    "SttPosition",
    "SttQtdata",
    "SttQtrans",
    "SttTdata",
    "SttTrans",
    "SttVideo",
    "Tdata",
    "Traj",
    "Transform",
    "Unit",
    "Video",
    "Videometa",
]
