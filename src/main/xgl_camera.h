#ifndef SRC_MAIN_XGL_CAMERA_H
#define SRC_MAIN_XGL_CAMERA_H

typedef union CameraVector {
    Vector4 vector;
    float element[4];
} CameraVector;

typedef struct CameraTravel {
    float focus;
    float fov;
    float fov_velocity;
    float scale;
    Vector4 rotation;
    Vector4 angle_velocity;
    Vector4 angle_delta;
    Vector4 position;
    Vector4 place_velocity;
    Vector4 place_delta;
    float chase_yaw;
    float chase_distance;
    unsigned char unmodeled_78[8];
    Vector4 chase_reference;
    Vector4 chase_position;
    Vector4 frustum[4];
} CameraTravel;

struct XglCameraScreen {
    unsigned int depthMax;
    unsigned int depthMin;
    unsigned char unmodeled_08[8];
    Vector4 clipRange;
    Vector4 portEffect;
    Vector4 resolution;
    Vector4 projection;
    Vector4 window;
    Vector4 screenOffset;
    Vector4 screenScale;
};

typedef struct CameraVolume {
    CameraVector vertex[16];
    CameraVector plane[12];
    Matrix4 frustum;
    Matrix4 viewMatrix;
    unsigned char unmodeled_240[0x40];
    float planeSet[4][4][4];
} CameraVolume;

struct XglCamera {
    unsigned int active;
    unsigned int state;
    unsigned char unmodeled_08[8];
    XglCameraScreen screen;
    CameraTravel travel;
    Matrix4 transform;
    Matrix4 inverseTransform;
    unsigned char unmodeled_1f0[0x80];
    CameraVolume volume[1];
};

#endif
