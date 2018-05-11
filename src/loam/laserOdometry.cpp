#include "laserOdometry.h"

namespace laserOdometry
{

laserOdometry::laserOdometry(tf::TransformBroadcaster * tfBroadcaster,tf::StampedTransform * laserOdometryTrans, ros::Publisher * pubLaserOdometry, ros::Publisher * pubLaserCloudLast2)
{
    this->tfBroadcaster = tfBroadcaster;
    this->laserOdometryTrans = laserOdometryTrans;
    this->pubLaserOdometry = pubLaserOdometry;
    this->pubLaserCloudLast2 = pubLaserCloudLast2;
    laserCloudExtreCur.reset(new pcl::PointCloud<pcl::PointXYZHSV>());
    laserCloudExtreLast.reset(new pcl::PointCloud<pcl::PointXYZHSV>());
    laserCloudExtreOri.reset(new pcl::PointCloud<pcl::PointXYZHSV>());
    laserCloudLast.reset(new pcl::PointCloud<pcl::PointXYZHSV>());
    laserCloudCornerLast.reset(new pcl::PointCloud<pcl::PointXYZHSV>());
    laserCloudSurfLast.reset(new pcl::PointCloud<pcl::PointXYZHSV>());
    laserCloudCornerLLast.reset(new pcl::PointCloud<pcl::PointXYZHSV>());
    laserCloudSurfLLast.reset(new pcl::PointCloud<pcl::PointXYZHSV>());
    coeffSel.reset(new pcl::PointCloud<pcl::PointXYZHSV>());
    kdtreeCornerLast.reset(new pcl::KdTreeFLANN<pcl::PointXYZHSV>());
    kdtreeSurfLast.reset(new pcl::KdTreeFLANN<pcl::PointXYZHSV>());
    kdtreeCornerLLast.reset(new pcl::KdTreeFLANN<pcl::PointXYZHSV>());
    kdtreeSurfLLast.reset(new pcl::KdTreeFLANN<pcl::PointXYZHSV>());
    systemInited = false;

    outLaserCloudLast2.reset(new pcl::PointCloud<pcl::PointXYZHSV>());
    //    transform[6] = {0};
    //    transformRec[6] = {0};
    //    transformSum[6] = {0};

    initTime = 0, timeLasted = 0, timeLastedRec = 0, startTimeCur = 0, startTimeLast = 0;

    imuRollStartCur = 0, imuPitchStartCur = 0, imuYawStartCur = 0;
    imuRollCur = 0, imuPitchCur = 0, imuYawCur = 0;
    imuShiftFromStartXCur = 0, imuShiftFromStartYCur = 0, imuShiftFromStartZCur = 0;
    imuVeloFromStartXCur = 0, imuVeloFromStartYCur = 0, imuVeloFromStartZCur = 0;

    imuRollStartLast = 0, imuPitchStartLast = 0, imuYawStartLast = 0;
    imuRollLast = 0, imuPitchLast = 0, imuYawLast = 0;
    imuShiftFromStartXLast = 0, imuShiftFromStartYLast = 0, imuShiftFromStartZLast = 0;
    imuVeloFromStartXLast = 0, imuVeloFromStartYLast = 0, imuVeloFromStartZLast = 0;
}

void laserOdometry::TransformReset()
{
    for (int i = 0; i < 6; i++) {
        transformRec[i] = transform[i];
        transform[i] = 0;
    }

    transformRec[3] -= imuVeloFromStartXLast * (startTimeCur - startTimeLast);
    transformRec[4] -= imuVeloFromStartYLast * (startTimeCur - startTimeLast);
    transformRec[5] -= imuVeloFromStartZLast * (startTimeCur - startTimeLast);
}

void laserOdometry::TransformToStart(pcl::PointXYZHSV *pi, pcl::PointXYZHSV *po, double startTime, double endTime)
{
    float s = (pi->h - startTime) / (endTime - startTime);
    s = 1;

    float rx = s * transform[0];
    float ry = s * transform[1];
    float rz = s * transform[2];
    float tx = s * transform[3];
    float ty = s * transform[4];
    float tz = s * transform[5];

    float x1 = cos(rz) * (pi->x - tx) + sin(rz) * (pi->y - ty);
    float y1 = -sin(rz) * (pi->x - tx) + cos(rz) * (pi->y - ty);
    float z1 = (pi->z - tz);

    float x2 = x1;
    float y2 = cos(rx) * y1 + sin(rx) * z1;
    float z2 = -sin(rx) * y1 + cos(rx) * z1;

    po->x = cos(ry) * x2 - sin(ry) * z2;
    po->y = y2;
    po->z = sin(ry) * x2 + cos(ry) * z2;
    po->h = pi->h;
    po->s = pi->s;
    po->v = pi->v;
}

void laserOdometry::TransformToEnd(pcl::PointXYZHSV *pi, pcl::PointXYZHSV *po, double startTime, double endTime)
{
    float s = (pi->h - startTime) / (endTime - startTime);
    s = 1;

    float rx = s * transform[0];
    float ry = s * transform[1];
    float rz = s * transform[2];
    float tx = s * transform[3];
    float ty = s * transform[4];
    float tz = s * transform[5];

    float x1 = cos(rz) * (pi->x - tx) + sin(rz) * (pi->y - ty);
    float y1 = -sin(rz) * (pi->x - tx) + cos(rz) * (pi->y - ty);
    float z1 = (pi->z - tz);

    float x2 = x1;
    float y2 = cos(rx) * y1 + sin(rx) * z1;
    float z2 = -sin(rx) * y1 + cos(rx) * z1;

    float x3 = cos(ry) * x2 - sin(ry) * z2;
    float y3 = y2;
    float z3 = sin(ry) * x2 + cos(ry) * z2;

    rx = transform[0];
    ry = transform[1];
    rz = transform[2];
    tx = transform[3];
    ty = transform[4];
    tz = transform[5];

    float x4 = cos(ry) * x3 + sin(ry) * z3;
    float y4 = y3;
    float z4 = -sin(ry) * x3 + cos(ry) * z3;

    float x5 = x4;
    float y5 = cos(rx) * y4 - sin(rx) * z4;
    float z5 = sin(rx) * y4 + cos(rx) * z4;

    float x6 = cos(rz) * x5 - sin(rz) * y5 + tx;
    float y6 = sin(rz) * x5 + cos(rz) * y5 + ty;
    float z6 = z5 + tz;

    float x7 = cos(imuRollStartLast) * (x6 - imuShiftFromStartXLast)
            - sin(imuRollStartLast) * (y6 - imuShiftFromStartYLast);
    float y7 = sin(imuRollStartLast) * (x6 - imuShiftFromStartXLast)
            + cos(imuRollStartLast) * (y6 - imuShiftFromStartYLast);
    float z7 = z6 - imuShiftFromStartZLast;

    float x8 = x7;
    float y8 = cos(imuPitchStartLast) * y7 - sin(imuPitchStartLast) * z7;
    float z8 = sin(imuPitchStartLast) * y7 + cos(imuPitchStartLast) * z7;

    float x9 = cos(imuYawStartLast) * x8 + sin(imuYawStartLast) * z8;
    float y9 = y8;
    float z9 = -sin(imuYawStartLast) * x8 + cos(imuYawStartLast) * z8;

    float x10 = cos(imuYawLast) * x9 - sin(imuYawLast) * z9;
    float y10 = y9;
    float z10 = sin(imuYawLast) * x9 + cos(imuYawLast) * z9;

    float x11 = x10;
    float y11 = cos(imuPitchLast) * y10 + sin(imuPitchLast) * z10;
    float z11 = -sin(imuPitchLast) * y10 + cos(imuPitchLast) * z10;

    po->x = cos(imuRollLast) * x11 + sin(imuRollLast) * y11;
    po->y = -sin(imuRollLast) * x11 + cos(imuRollLast) * y11;
    po->z = z11;
    po->h = pi->h;
    po->s = pi->s;
    po->v = pi->v;
}

void laserOdometry::AccumulateRotation(float cx, float cy, float cz, float lx, float ly, float lz,
                                       float &ox, float &oy, float &oz)
{
    float srx = cos(lx)*cos(cx)*sin(ly)*sin(cz) - cos(cx)*cos(cz)*sin(lx) - cos(lx)*cos(ly)*sin(cx);
    ox = -asin(srx);

    float srycrx = sin(lx)*(cos(cy)*sin(cz) - cos(cz)*sin(cx)*sin(cy)) + cos(lx)*sin(ly)*(cos(cy)*cos(cz)
                                                                                          + sin(cx)*sin(cy)*sin(cz)) + cos(lx)*cos(ly)*cos(cx)*sin(cy);
    float crycrx = cos(lx)*cos(ly)*cos(cx)*cos(cy) - cos(lx)*sin(ly)*(cos(cz)*sin(cy)
                                                                      - cos(cy)*sin(cx)*sin(cz)) - sin(lx)*(sin(cy)*sin(cz) + cos(cy)*cos(cz)*sin(cx));
    oy = atan2(srycrx / cos(ox), crycrx / cos(ox));

    float srzcrx = sin(cx)*(cos(lz)*sin(ly) - cos(ly)*sin(lx)*sin(lz)) + cos(cx)*sin(cz)*(cos(ly)*cos(lz)
                                                                                          + sin(lx)*sin(ly)*sin(lz)) + cos(lx)*cos(cx)*cos(cz)*sin(lz);
    float crzcrx = cos(lx)*cos(lz)*cos(cx)*cos(cz) - cos(cx)*sin(cz)*(cos(ly)*sin(lz)
                                                                      - cos(lz)*sin(lx)*sin(ly)) - sin(cx)*(sin(ly)*sin(lz) + cos(ly)*cos(lz)*sin(lx));
    oz = atan2(srzcrx / cos(ox), crzcrx / cos(ox));
}

void laserOdometry::PluginIMURotation(float bcx, float bcy, float bcz, float blx, float bly, float blz,
                                      float alx, float aly, float alz, float &acx, float &acy, float &acz)
{
    float sbcx = sin(bcx);
    float cbcx = cos(bcx);
    float sbcy = sin(bcy);
    float cbcy = cos(bcy);
    float sbcz = sin(bcz);
    float cbcz = cos(bcz);

    float sblx = sin(blx);
    float cblx = cos(blx);
    float sbly = sin(bly);
    float cbly = cos(bly);
    float sblz = sin(blz);
    float cblz = cos(blz);

    float salx = sin(alx);
    float calx = cos(alx);
    float saly = sin(aly);
    float caly = cos(aly);
    float salz = sin(alz);
    float calz = cos(alz);

    float srx = -sbcx*(salx*sblx + calx*caly*cblx*cbly + calx*cblx*saly*sbly)
            - cbcx*cbcz*(calx*saly*(cbly*sblz - cblz*sblx*sbly)
                         - calx*caly*(sbly*sblz + cbly*cblz*sblx) + cblx*cblz*salx)
            - cbcx*sbcz*(calx*caly*(cblz*sbly - cbly*sblx*sblz)
                         - calx*saly*(cbly*cblz + sblx*sbly*sblz) + cblx*salx*sblz);
    acx = -asin(srx);

    float srycrx = (cbcy*sbcz - cbcz*sbcx*sbcy)*(calx*saly*(cbly*sblz - cblz*sblx*sbly)
                                                 - calx*caly*(sbly*sblz + cbly*cblz*sblx) + cblx*cblz*salx)
            - (cbcy*cbcz + sbcx*sbcy*sbcz)*(calx*caly*(cblz*sbly - cbly*sblx*sblz)
                                            - calx*saly*(cbly*cblz + sblx*sbly*sblz) + cblx*salx*sblz)
            + cbcx*sbcy*(salx*sblx + calx*caly*cblx*cbly + calx*cblx*saly*sbly);
    float crycrx = (cbcz*sbcy - cbcy*sbcx*sbcz)*(calx*caly*(cblz*sbly - cbly*sblx*sblz)
                                                 - calx*saly*(cbly*cblz + sblx*sbly*sblz) + cblx*salx*sblz)
            - (sbcy*sbcz + cbcy*cbcz*sbcx)*(calx*saly*(cbly*sblz - cblz*sblx*sbly)
                                            - calx*caly*(sbly*sblz + cbly*cblz*sblx) + cblx*cblz*salx)
            + cbcx*cbcy*(salx*sblx + calx*caly*cblx*cbly + calx*cblx*saly*sbly);
    acy = atan2(srycrx / cos(acx), crycrx / cos(acx));

    float srzcrx = sbcx*(cblx*cbly*(calz*saly - caly*salx*salz)
                         - cblx*sbly*(caly*calz + salx*saly*salz) + calx*salz*sblx)
            - cbcx*cbcz*((caly*calz + salx*saly*salz)*(cbly*sblz - cblz*sblx*sbly)
                         + (calz*saly - caly*salx*salz)*(sbly*sblz + cbly*cblz*sblx)
                         - calx*cblx*cblz*salz) + cbcx*sbcz*((caly*calz + salx*saly*salz)*(cbly*cblz
                                                                                           + sblx*sbly*sblz) + (calz*saly - caly*salx*salz)*(cblz*sbly - cbly*sblx*sblz)
                                                             + calx*cblx*salz*sblz);
    float crzcrx = sbcx*(cblx*sbly*(caly*salz - calz*salx*saly)
                         - cblx*cbly*(saly*salz + caly*calz*salx) + calx*calz*sblx)
            + cbcx*cbcz*((saly*salz + caly*calz*salx)*(sbly*sblz + cbly*cblz*sblx)
                         + (caly*salz - calz*salx*saly)*(cbly*sblz - cblz*sblx*sbly)
                         + calx*calz*cblx*cblz) - cbcx*sbcz*((saly*salz + caly*calz*salx)*(cblz*sbly
                                                                                           - cbly*sblx*sblz) + (caly*salz - calz*salx*saly)*(cbly*cblz + sblx*sbly*sblz)
                                                             - calx*calz*cblx*sblz);
    acz = atan2(srzcrx / cos(acx), crzcrx / cos(acx));
}

void laserOdometry::laserCloudExtreCurHandler(const sensor_msgs::PointCloud2& laserCloudExtreCur2)
{
    if (!systemInited) {
        initTime = laserCloudExtreCur2.header.stamp.toSec();
        systemInited = true;
    }
    timeLaserCloudExtreCur = laserCloudExtreCur2.header.stamp.toSec();
    timeLasted = timeLaserCloudExtreCur - initTime;

    pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudExtreCur3(new pcl::PointCloud<pcl::PointXYZHSV>());
    pcl::fromROSMsg(laserCloudExtreCur2, *laserCloudExtreCur3);

    int laserCloudExtreCur3Size = laserCloudExtreCur3->points.size();

    laserCloudExtreCur->clear();
    for (int i = 0; i < laserCloudExtreCur3Size; i++) {
        if (fabs(laserCloudExtreCur3->points[i].v - 10) < 0.005) {
            imuPitchStartCur = laserCloudExtreCur3->points[i].x;
            imuYawStartCur = laserCloudExtreCur3->points[i].y;
            imuRollStartCur = laserCloudExtreCur3->points[i].z;
        } else if (fabs(laserCloudExtreCur3->points[i].v - 11) < 0.005) {
            imuPitchCur = laserCloudExtreCur3->points[i].x;
            imuYawCur = laserCloudExtreCur3->points[i].y;
            imuRollCur = laserCloudExtreCur3->points[i].z;
        } else if (fabs(laserCloudExtreCur3->points[i].v - 12) < 0.005) {
            imuShiftFromStartXCur = laserCloudExtreCur3->points[i].x;
            imuShiftFromStartYCur = laserCloudExtreCur3->points[i].y;
            imuShiftFromStartZCur = laserCloudExtreCur3->points[i].z;
        } else if (fabs(laserCloudExtreCur3->points[i].v - 13) < 0.005) {
            imuVeloFromStartXCur = laserCloudExtreCur3->points[i].x;
            imuVeloFromStartYCur = laserCloudExtreCur3->points[i].y;
            imuVeloFromStartZCur = laserCloudExtreCur3->points[i].z;
        } else {
            laserCloudExtreCur->push_back(laserCloudExtreCur3->points[i]);
        }
    }
    std::cout << "copied laserCloudExtreCur there are now " << laserCloudExtreCur->size() << std::endl;
    laserCloudExtreCur3->clear();

    if (!imuInited) {
        transformSum[0] += imuPitchStartCur;
        //transformSum[1] += imuYawStartCur;
        transformSum[2] += imuRollStartCur;

        imuInited = true;
    }

    //    if (timeLasted > 4.0)
    newLaserCloudExtreCur = true;
    //    else
    //        std::cout << "Extre timeLasted > 4.0" << std::endl;
}

void laserOdometry::laserCloudExtreCurHandlerVelo(const pcl::PointCloud<pcl::PointXYZHSV>::Ptr inLaserCloudExtreCur3)
{
    systemInited = true;


    pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudExtreCur3(new pcl::PointCloud<pcl::PointXYZHSV>());

    *laserCloudExtreCur3 = *inLaserCloudExtreCur3;
    int laserCloudExtreCur3Size = laserCloudExtreCur3->points.size();

    laserCloudExtreCur->clear();

    *laserCloudExtreCur = * laserCloudExtreCur3;
    laserCloudExtreCur3->clear();

    if (!imuInited) {
        transformSum[0] += imuPitchStartCur;
        //transformSum[1] += imuYawStartCur;
        transformSum[2] += imuRollStartCur;

        imuInited = true;
    }
    newLaserCloudExtreCur = true;
}

void laserOdometry::laserCloudLastHandler(const sensor_msgs::PointCloud2 &laserCloudLast2)
{
    // if (laserCloudLast2.header.stamp.toSec() > timeLaserCloudLast + 0.005)
    //  {
    timeLaserCloudLast = laserCloudLast2.header.stamp.toSec();
    startTimeLast = startTimeCur;
    startTimeCur = timeLaserCloudLast - initTime;

    pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudPointer = laserCloudCornerLLast;
    laserCloudCornerLLast = laserCloudCornerLast;
    laserCloudCornerLast = laserCloudPointer; /// bulshit?

    laserCloudPointer = laserCloudSurfLLast;
    laserCloudSurfLLast = laserCloudSurfLast;
    laserCloudSurfLast = laserCloudPointer;

    laserCloudLast->clear();
    pcl::fromROSMsg(laserCloudLast2, *laserCloudLast);
    int laserCloudLastSize = laserCloudLast->points.size();

    laserCloudExtreLast->clear();
    laserCloudCornerLast->clear();
    laserCloudSurfLast->clear();
    for (int i = 0; i < laserCloudLastSize; i++) {
        // is v=2 or v=-1
        if (fabs(laserCloudLast->points[i].v - 2) < 0.005 || fabs(laserCloudLast->points[i].v + 1) < 0.005) {
            laserCloudExtreLast->push_back(laserCloudLast->points[i]);
        }
        // is v=2 or v=1
        if (fabs(laserCloudLast->points[i].v - 2) < 0.005 || fabs(laserCloudLast->points[i].v - 1) < 0.005) {
            laserCloudCornerLast->push_back(laserCloudLast->points[i]);
        }
        // is v=0 or v=-1
        if (fabs(laserCloudLast->points[i].v) < 0.005 || fabs(laserCloudLast->points[i].v + 1) < 0.005) {
            laserCloudSurfLast->push_back(laserCloudLast->points[i]);
        }
        //        if (fabs(laserCloudLast->points[i].v - 10) < 0.005) {
        //            imuPitchStartLast = laserCloudLast->points[i].x;
        //            imuYawStartLast = laserCloudLast->points[i].y;
        //            imuRollStartLast = laserCloudLast->points[i].z;
        //        }
        //        if (fabs(laserCloudLast->points[i].v - 11) < 0.005) {
        //            imuPitchLast = laserCloudLast->points[i].x;
        //            imuYawLast = laserCloudLast->points[i].y;
        //            imuRollLast = laserCloudLast->points[i].z;
        //        }
        //        if (fabs(laserCloudLast->points[i].v - 12) < 0.005) {
        //            imuShiftFromStartXLast = laserCloudLast->points[i].x;
        //            imuShiftFromStartYLast = laserCloudLast->points[i].y;
        //            imuShiftFromStartZLast = laserCloudLast->points[i].z;
        //        }
        //        if (fabs(laserCloudLast->points[i].v - 13) < 0.005) {
        //            imuVeloFromStartXLast = laserCloudLast->points[i].x;
        //            imuVeloFromStartYLast = laserCloudLast->points[i].y;
        //            imuVeloFromStartZLast = laserCloudLast->points[i].z;
        //        }
    }
    std::cout << "copied laserCloudExtreLast " << laserCloudExtreLast->size() << std::endl;
    std::cout << "copied laserCloudCornerLast " << laserCloudCornerLast->size() << std::endl;
    std::cout << "copied laserCloudSurfLast " << laserCloudSurfLast->size() << std::endl;

    pcl::KdTreeFLANN<pcl::PointXYZHSV>::Ptr kdtreePointer = kdtreeCornerLLast;
    kdtreeCornerLLast = kdtreeCornerLast;
    kdtreeCornerLast = kdtreePointer;
    kdtreeCornerLast->setInputCloud(laserCloudCornerLast);

    kdtreePointer = kdtreeSurfLLast;
    kdtreeSurfLLast = kdtreeSurfLast;
    kdtreeSurfLast = kdtreePointer;
    kdtreeSurfLast->setInputCloud(laserCloudSurfLast);

    //        if (timeLasted > 4.0)
    newLaserCloudLast = true;
    //        else
    //            std::cout << "timeLasted < 4.0" << std::endl;
    //    }
    //    else
    //    {
    //        std::cout << "New Point cloud arived to early" << std::endl;
    //    }
}

void laserOdometry::laserCloudLastHandlerVelo(const pcl::PointCloud<pcl::PointXYZHSV>::Ptr cornerPointsSharp, const pcl::PointCloud<pcl::PointXYZHSV>::Ptr cornerPointsLessSharp, const pcl::PointCloud<pcl::PointXYZHSV>::Ptr surfPointsFlat, const pcl::PointCloud<pcl::PointXYZHSV>::Ptr surfPointsLessFlatDS)
{

    pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudPointer = laserCloudCornerLLast;
    laserCloudCornerLLast = laserCloudCornerLast;
    laserCloudCornerLast = laserCloudPointer; /// bulshit?

    laserCloudPointer = laserCloudSurfLLast;
    laserCloudSurfLLast = laserCloudSurfLast;
    laserCloudSurfLast = laserCloudPointer;




    laserCloudExtreLast->clear();
    laserCloudCornerLast->clear();
    laserCloudSurfLast->clear();

    // v==2 cornerPointsSharp
    // v==1 cornerPointsLessSharp
    // v==-1 surfPointsFlat
    // v==0 surfPointsLessFlat surfPointsLessFlatDS
    // is v=2 or v=-1
    *laserCloudExtreLast = *cornerPointsSharp;
    *laserCloudExtreLast += *surfPointsFlat;

    // is v=2 or v=1
    *laserCloudCornerLast = *cornerPointsSharp;
    *laserCloudCornerLast += *cornerPointsLessSharp;

    // is v=0 or v=-1
    *laserCloudSurfLast = *surfPointsLessFlatDS;
    *laserCloudSurfLast += *surfPointsFlat;




    pcl::KdTreeFLANN<pcl::PointXYZHSV>::Ptr kdtreePointer = kdtreeCornerLLast;
    kdtreeCornerLLast = kdtreeCornerLast;
    kdtreeCornerLast = kdtreePointer;
    kdtreeCornerLast->setInputCloud(laserCloudCornerLast);

    kdtreePointer = kdtreeSurfLLast;
    kdtreeSurfLLast = kdtreeSurfLast;
    kdtreeSurfLast = kdtreePointer;
    kdtreeSurfLast->setInputCloud(laserCloudSurfLast);

    newLaserCloudLast = true;
}

void laserOdometry::main_laserOdometry(sensor_msgs::PointCloud2 &pub, nav_msgs::Odometry &pubOdo)
{
    nav_msgs::Odometry laserOdometry;
    laserOdometry.header.frame_id = "/camera_init";
    laserOdometry.child_frame_id = "/camera";

    //tf::TransformBroadcaster tfBroadcaster;
    //tf::StampedTransform laserOdometryTrans;
    laserOdometryTrans->frame_id_ = "/camera_init";
    laserOdometryTrans->child_frame_id_ = "/camera";

    bool sweepEnd = false;
    bool newLaserPoints = false;
    bool sufficientPoints = false;
    double startTime, endTime;
    pcl::PointCloud<pcl::PointXYZHSV>::Ptr extrePointsPtr, laserCloudCornerPtr, laserCloudSurfPtr;
    pcl::KdTreeFLANN<pcl::PointXYZHSV>::Ptr kdtreeCornerPtr, kdtreeSurfPtr;
    if (newLaserCloudExtreCur && newLaserCloudLast)
    {
        std::cout << "newLaserCloudExtreCur && newLaserCloudLast" << std::endl;
        startTime = startTimeLast;
        endTime = startTimeCur;

        //        extrePointsPtr = laserCloudExtreLast;
        //        laserCloudCornerPtr = laserCloudCornerLLast;
        //        laserCloudSurfPtr = laserCloudSurfLLast;
        //        kdtreeCornerPtr = kdtreeCornerLLast;
        //        kdtreeSurfPtr = kdtreeSurfLLast;

        extrePointsPtr = laserCloudExtreCur;
        laserCloudCornerPtr = laserCloudCornerLast;
        laserCloudSurfPtr = laserCloudSurfLast;
        kdtreeCornerPtr = kdtreeCornerLast;
        kdtreeSurfPtr = kdtreeSurfLast;

        laserOdometry.header.stamp = ros::Time().fromSec(timeLaserCloudLast);
        laserOdometryTrans->stamp_ = ros::Time().fromSec(timeLaserCloudLast);

        sweepEnd = true;
        newLaserPoints = true;

        if (laserCloudSurfPtr->points.size() >= 100) {
            sufficientPoints = true;
        }
        else
        {
            std::cout << "Not enough surf points (" << laserCloudSurfLLast->points.size() << ")" << std::endl;
        }

    } else if (newLaserCloudExtreCur)
    {
        std::cout << "newLaserCloudExtreCur" << std::endl;
        startTime = startTimeCur;
        endTime = timeLasted;

        extrePointsPtr = laserCloudExtreCur;
        laserCloudCornerPtr = laserCloudCornerLast;
        laserCloudSurfPtr = laserCloudSurfLast;
        kdtreeCornerPtr = kdtreeCornerLast;
        kdtreeSurfPtr = kdtreeSurfLast;

        laserOdometry.header.stamp = ros::Time().fromSec(timeLaserCloudExtreCur);
        laserOdometryTrans->stamp_ = ros::Time().fromSec(timeLaserCloudExtreCur);

        float s = (timeLasted - timeLastedRec) / (startTimeCur - startTimeLast);
        for (int i = 0; i < 6; i++) {
            transform[i] += s * transformRec[i];
        }
        timeLastedRec = timeLasted;

        newLaserPoints = true;

        if (laserCloudSurfLast->points.size() >= 100) {
            sufficientPoints = true;
        }
    }



    if (newLaserPoints && sufficientPoints)
    {
        std::cout << "newPoints and enough Points" << std::endl;
        newLaserCloudExtreCur = false;
        newLaserCloudLast = false;


        int laserCloudCornerNum = laserCloudCornerPtr->points.size();
        int laserCloudSurfNum = laserCloudSurfPtr->points.size();
        int extrePointNum = extrePointsPtr->points.size();
        std::cout << "extrePointNum=" << extrePointNum << std::endl;

        float st = 1;
        if (!sweepEnd) {
            st = (timeLasted - startTime) / (startTimeCur - startTimeLast);
        }
        st = 50;
        int iterNum = 750;

        int pointSelSkipNum = 2;

        /// a number of iterations
        std::cout << "Will process " << iterNum << " iterations" << std::endl;

        for (int iterCount = 0; iterCount < iterNum; iterCount++)
        {
            laserCloudExtreOri->clear();
            //laserCloudExtreSel->clear();
            //laserCloudExtreUnsel->clear();
            //laserCloudExtreProj->clear();
            //laserCloudSel->clear();
            coeffSel->clear();

            bool isPointSel = false;
            if (iterCount % (pointSelSkipNum + 1) == 0) {
                isPointSel = true;
                pointSelInd.clear();
            }

            err_edge_pointSearchSqDis=0;
            err_edge_dist=0;
            err_edge_pointSelInd=0;
            err_edge_minPointInd2 =0;
            err_planar_pointSearchSqDis=0;
            err_planar_dist=0;
            err_planar_pointSelInd=0;
            number_planar=0;
            number_edge=0;
            for (int i = 0; i < extrePointNum; i++)
            {
                extreOri = extrePointsPtr->points[i];
                //                if (i==0)
                //                {
                //                    std::cout << "extreOri" << extreOri << std::endl;
                //                    std::cout << "extreSel" << extreSel << std::endl;
                //                    std::cout << "rx" << transform[0]
                //                              << ", ry" << transform[1]
                //                              << ", rz" << transform[2]
                //                              << ", tx" << transform[3]
                //                              << ", ty" << transform[4]
                //                              << ", tz" << transform[5]
                //                              << std::endl;
                //                }
                TransformToStart(&extreOri, &extreSel, startTime, endTime);
                //                if (i==0)
                //                {
                //                    std::cout << "-------------" << std::endl;
                //                    std::cout << "extreOri" << extreOri << std::endl;
                //                    std::cout << "extreSel" << extreSel << std::endl;
                //                    std::cout << "rx" << transform[0]
                //                              << ", ry" << transform[1]
                //                              << ", rz" << transform[2]
                //                              << ", tx" << transform[3]
                //                              << ", ty" << transform[4]
                //                              << ", tz" << transform[5]
                //                              << std::endl;
                //                    std::cout << "isPointSel" << isPointSel << std::endl;
                //                }

                if (isPointSel) {
                    pointSelInd.push_back(-1);
                    pointSelInd.push_back(-1);
                    pointSelInd.push_back(-1);
                }

                /// edge point
                if (fabs(extreOri.v + 1) < 0.05)
                {
                    int ret = processEdgePoint(kdtreeSurfPtr, laserCloudSurfPtr, isPointSel, laserCloudSurfNum, i, iterCount, iterNum);
                    if (ret==-1)
                        //std::cout << "pointSearchSqDis[0]=" << pointSearchSqDis[0] << " > 1.0" << std::endl;
                        err_edge_pointSearchSqDis++;
                    if (ret==-2)
                        //std::cout << "dist > 1.0" << std::endl;
                        err_edge_dist++;
                    if (ret==-3)
                        //std::cout << "pointSelInd[3 * i]=" << pointSelInd[3 * i] << " >= 0" << std::endl;
                        err_edge_pointSelInd++;
                    if (ret==-4)
                        err_edge_minPointInd2++;
                }
                /// planar point
                else if (fabs(extreOri.v - 2) < 0.05)
                {
                    int ret = processPlanarPoint(kdtreeCornerPtr, laserCloudCornerPtr, isPointSel, laserCloudCornerNum, i);
                    if (ret==-1)
                        err_planar_pointSearchSqDis++;
                    if (ret==-2)
                        err_planar_dist++;
                    if (ret==-3)
                        err_planar_pointSelInd++;
                }
            }

            //            std::cout << "err_edge_pointSearchSqDis=" << err_edge_pointSearchSqDis
            //                      << ", err_edge_dist=" << err_edge_dist
            //                      << ", err_edge_pointSelInd=" << err_edge_pointSelInd
            //                      << ", err_edge_pointSelInd=" << err_edge_pointSelInd
            //                      << ", err_planar_pointSearchSqDis=" << err_planar_pointSearchSqDis
            //                      << ", err_planar_dist=" << err_planar_dist
            //                      << ", err_planar_pointSelInd=" << err_planar_pointSelInd
            //                      << ", number_planar" << number_planar
            //                      << ", number_edge=" << number_edge
            //                      << std::endl;



            int extrePointSelNum = laserCloudExtreOri->points.size();
            if (extrePointSelNum < 10) {
                ROS_WARN ("extrePointSelNum < 10: extrePointSelNum=%d, number_planar=%d, number_edge=%d", extrePointSelNum,number_planar,number_edge);
                continue;
            }

            cv::Mat matA(extrePointSelNum, 6, CV_32F, cv::Scalar::all(0));
            cv::Mat matAt(6, extrePointSelNum, CV_32F, cv::Scalar::all(0));
            cv::Mat matAtA(6, 6, CV_32F, cv::Scalar::all(0));
            cv::Mat matB(extrePointSelNum, 1, CV_32F, cv::Scalar::all(0));
            cv::Mat matAtB(6, 1, CV_32F, cv::Scalar::all(0));
            cv::Mat matX(6, 1, CV_32F, cv::Scalar::all(0));
            for (int i = 0; i < extrePointSelNum; i++) {
                extreOri = laserCloudExtreOri->points[i];
                coeff = coeffSel->points[i];

                float s = (extreOri.h - startTime) / (endTime - startTime);
                s=1;

                float srx = sin(s * transform[0]);
                float crx = cos(s * transform[0]);
                float sry = sin(s * transform[1]);
                float cry = cos(s * transform[1]);
                float srz = sin(s * transform[2]);
                float crz = cos(s * transform[2]);
                float tx = s * transform[3];
                float ty = s * transform[4];
                float tz = s * transform[5];

                float arx = (-s*crx*sry*srz*extreOri.x + s*crx*crz*sry*extreOri.y + s*srx*sry*extreOri.z
                             + s*tx*crx*sry*srz - s*ty*crx*crz*sry - s*tz*srx*sry) * coeff.x
                        + (s*srx*srz*extreOri.x - s*crz*srx*extreOri.y + s*crx*extreOri.z
                           + s*ty*crz*srx - s*tz*crx - s*tx*srx*srz) * coeff.y
                        + (s*crx*cry*srz*extreOri.x - s*crx*cry*crz*extreOri.y - s*cry*srx*extreOri.z
                           + s*tz*cry*srx + s*ty*crx*cry*crz - s*tx*crx*cry*srz) * coeff.z;
                if (std::isnan(arx) || std::isnan(-arx))
                    ROS_ERROR ("arx=%f, s=%f, crx=%f, sry=%f, srz=%f, extreOri.x=%f, extreOri.y=%f, extreOri.z=%f, coeff.x=%f, coeff.y=%f, coeff.z=%f", arx, s, crx, sry, srz, extreOri.x, extreOri.y, extreOri.z, coeff.x, coeff.y, coeff.z);

                float ary = ((-s*crz*sry - s*cry*srx*srz)*extreOri.x
                             + (s*cry*crz*srx - s*sry*srz)*extreOri.y - s*crx*cry*extreOri.z
                             + tx*(s*crz*sry + s*cry*srx*srz) + ty*(s*sry*srz - s*cry*crz*srx)
                             + s*tz*crx*cry) * coeff.x
                        + ((s*cry*crz - s*srx*sry*srz)*extreOri.x
                           + (s*cry*srz + s*crz*srx*sry)*extreOri.y - s*crx*sry*extreOri.z
                           + s*tz*crx*sry - ty*(s*cry*srz + s*crz*srx*sry)
                           - tx*(s*cry*crz - s*srx*sry*srz)) * coeff.z;

                float arz = ((-s*cry*srz - s*crz*srx*sry)*extreOri.x + (s*cry*crz - s*srx*sry*srz)*extreOri.y
                             + tx*(s*cry*srz + s*crz*srx*sry) - ty*(s*cry*crz - s*srx*sry*srz)) * coeff.x
                        + (-s*crx*crz*extreOri.x - s*crx*srz*extreOri.y
                           + s*ty*crx*srz + s*tx*crx*crz) * coeff.y
                        + ((s*cry*crz*srx - s*sry*srz)*extreOri.x + (s*crz*sry + s*cry*srx*srz)*extreOri.y
                           + tx*(s*sry*srz - s*cry*crz*srx) - ty*(s*crz*sry + s*cry*srx*srz)) * coeff.z;

                float atx = -s*(cry*crz - srx*sry*srz) * coeff.x + s*crx*srz * coeff.y
                        - s*(crz*sry + cry*srx*srz) * coeff.z;

                float aty = -s*(cry*srz + crz*srx*sry) * coeff.x - s*crx*crz * coeff.y
                        - s*(sry*srz - cry*crz*srx) * coeff.z;

                float atz = s*crx*sry * coeff.x - s*srx * coeff.y - s*crx*cry * coeff.z;

                float d2 = coeff.h;

                matA.at<float>(i, 0) = arx;
                matA.at<float>(i, 1) = ary;
                matA.at<float>(i, 2) = arz;
                matA.at<float>(i, 3) = atx;
                matA.at<float>(i, 4) = aty;
                matA.at<float>(i, 5) = atz;
                matB.at<float>(i, 0) = -0.015 * st * d2;
                //                std::cout << "arx=" << arx
                //                          << ", ary=" << ary
                //                          << ", arz=" << arz
                //                          << ", atx=" << atx
                //                          << ", aty=" << aty
                //                          << ", atz=" << atz << std::endl;
                if(std::isnan(matB.at<float>(i, 0)) || std::isnan(-matB.at<float>(i, 0)))
                    ROS_ERROR ("matB.at<float>(i, 0)=%f",  matB.at<float>(i, 0));
            }
            cv::transpose(matA, matAt);
            matAtA = matAt * matA; //+ 0.1 * cv::Mat::eye(6, 6, CV_32F);
            matAtB = matAt * matB;

            if(!cv::solve(matAtA, matAtB, matX, cv::DECOMP_QR))
            {
                ROS_ERROR ("solve error");
                for (int x=0;x<6;x++)
                    for (int y=0;y<6;y++)
                        std::cout << "matAtA.at<float>(x,y)=" << matAtA.at<float>(x,y) << std::endl;
                for (int x=0;x<6;x++)
                    std::cout << "matAtB.at<float>(x,1)=" << matAtB.at<float>(x,1) << std::endl;
            }
            //cv::solve(matA, matB, matX, cv::DECOMP_SVD);

            if (fabs(matX.at<float>(0, 0)) < 0.1 &&
                    fabs(matX.at<float>(1, 0)) < 0.1 &&
                    fabs(matX.at<float>(2, 0)) < 0.1 &&
                    fabs(matX.at<float>(3, 0)) < 0.9 &&
                    fabs(matX.at<float>(4, 0)) < 0.9 &&
                    fabs(matX.at<float>(5, 0)) < 0.9) {

                //transform[0] += 0.7 * matX.at<float>(0, 0);
                //transform[1] += 0.7 * matX.at<float>(1, 0);
                //transform[2] += 0.7 * matX.at<float>(2, 0);
                transform[0] += 0.1 * matX.at<float>(0, 0);
                transform[1] += 0.1 * matX.at<float>(1, 0);
                transform[2] += 0.1 * matX.at<float>(2, 0);
                transform[3] += matX.at<float>(3, 0);
                transform[4] += matX.at<float>(4, 0);
                transform[5] += matX.at<float>(5, 0);
            } else {
                ROS_ERROR ("Odometry update out of bound: tx=%f, ty=%f, tz=%f",matX.at<float>(3, 0),matX.at<float>(4, 0),matX.at<float>(5, 0));
                //                for (int x=0;x<6;x++)
                //                    for (int y=0;y<6;y++)
                //                        std::cout << "matAtA.at<float>(x,y)=" << matAtA.at<float>(x,y) << std::endl;
                //                for (int x=0;x<6;x++)
                //                    std::cout << "matAtB.at<float>(x,1)=" << matAtB.at<float>(x,1) << std::endl;
                //                for (int x=0;x<extrePointNum;x++)
                //                    for (int y=0;y<6;y++)
                //                    {
                //                        if (isnan(matA.at<float>(x,y)) || isnan(-matA.at<float>(x,y)))
                //                            std::cout << "matA.at<float>(" << x << "," << y << ")=" << matA.at<float>(x,y) << std::endl;
                //                    }
                //                for (int x=0;x<extrePointNum;x++)
                //                {
                //                    if (isnan(matB.at<float>(x,0)) || isnan(-matB.at<float>(x,0)))
                //                        std::cout << "matB.at<float>(" << x << ",0)=" << matB.at<float>(x,0) << std::endl;
                //                }
            }

            float deltaR = sqrt(matX.at<float>(0, 0) * 180 / PI * matX.at<float>(0, 0) * 180 / PI
                                + matX.at<float>(1, 0) * 180 / PI * matX.at<float>(1, 0) * 180 / PI
                                + matX.at<float>(2, 0) * 180 / PI * matX.at<float>(2, 0) * 180 / PI);
            float deltaT = sqrt(matX.at<float>(3, 0) * 100 * matX.at<float>(3, 0) * 100
                                + matX.at<float>(4, 0) * 100 * matX.at<float>(4, 0) * 100
                                + matX.at<float>(5, 0) * 100 * matX.at<float>(5, 0) * 100);

            if (deltaR < 0.1 && deltaT < 0.1) {
                ROS_WARN ("deltaR=%f < 0.005 && deltaT=%f < 0.001", deltaR, deltaT);
                break;
            }

            ROS_INFO ("iter: %d, deltaR: %f, deltaT: %f", iterCount, deltaR, deltaT);
        }
    }

    /// Last step
    if (newLaserPoints)
    {
        //std::cout << "newLaserPoints" << std::endl;
        float rx=0, ry=0, rz=0, tx=0, ty=0, tz=0;
        double magic = 1.00;
        AccumulateRotation(transformSum[0], transformSum[1], transformSum[2], -transform[0], -transform[1] * magic, -transform[2], rx, ry, rz);

        float x1, y1, z1;
        if (sweepEnd)
        {
            x1 = cos(rz) * (transform[3] - imuShiftFromStartXLast) - sin(rz) * (transform[4] - imuShiftFromStartYLast);
            y1 = sin(rz) * (transform[3] - imuShiftFromStartXLast) + cos(rz) * (transform[4] - imuShiftFromStartYLast);
            z1 = transform[5] * magic - imuShiftFromStartZLast;
        }
        else
        {
            x1 = cos(rz) * (transform[3] - imuShiftFromStartXCur) - sin(rz) * (transform[4] - imuShiftFromStartYCur);
            y1 = sin(rz) * (transform[3] - imuShiftFromStartXCur) + cos(rz) * (transform[4] - imuShiftFromStartYCur);
            z1 = transform[5] * magic - imuShiftFromStartZCur;
        }

        float x2 = x1;
        float y2 = cos(rx) * y1 - sin(rx) * z1;
        float z2 = sin(rx) * y1 + cos(rx) * z1;

        tx = transformSum[3] - (cos(ry) * x2 + sin(ry) * z2);
        ty = transformSum[4] - y2;
        tz = transformSum[5] - (-sin(ry) * x2 + cos(ry) * z2);

        setTransformationMatrix(transform[0],transform[1],transform[2],transform[3],transform[4],transform[5]);

        // at the end of a sweep
        if (sweepEnd)
        {
            PluginIMURotation(rx, ry, rz, imuPitchStartLast, imuYawStartLast, imuRollStartLast,
                              imuPitchLast, imuYawLast, imuRollLast, rx, ry, rz);

            //            int laserCloudCornerLastNum = laserCloudCornerLast->points.size();
            //            for (int i = 0; i < laserCloudCornerLastNum; i++) {
            //                TransformToEnd(&laserCloudCornerLast->points[i], &laserCloudCornerLast->points[i],
            //                               startTimeLast, startTimeCur);
            //                //std::cout << "laserCloudCornerLast->points[i]=" << laserCloudCornerLast->points[i] << std::endl;
            //            }

            //            int laserCloudSurfLastNum = laserCloudSurfLast->points.size();
            //            for (int i = 0; i < laserCloudSurfLastNum; i++) {
            //                //std::cout << "laserCloudSurfLast->points[i]=" << laserCloudSurfLast->points[i] << std::endl;
            //                TransformToEnd(&laserCloudSurfLast->points[i], &laserCloudSurfLast->points[i],
            //                               startTimeLast, startTimeCur);
            //                //std::cout << "laserCloudSurfLast->points[i]=" << laserCloudSurfLast->points[i] << std::endl;
            //            }




            TransformReset();

            transformSum[0] = rx;
            transformSum[1] = ry;
            transformSum[2] = rz;
            transformSum[3] = tx;
            transformSum[4] = ty;
            transformSum[5] = tz;



            *outLaserCloudLast2 = *laserCloudCornerLast;
            *outLaserCloudLast2 += *laserCloudSurfLast;
            //sensor_msgs::PointCloud2 laserCloudLast2;
            //pcl::toROSMsg(*outLaserCloudLast2, laserCloudLast2);
            //laserCloudLast2.header.stamp = ros::Time().fromSec(timeLaserCloudLast);
            //laserCloudLast2.header.frame_id = "/camera";
            //pub = laserCloudLast2;
            //pubLaserCloudLast2->publish(laserCloudLast2);

        } else {

//            PluginIMURotation(rx, ry, rz, imuPitchStartCur, imuYawStartCur, imuRollStartCur,
//                              imuPitchCur, imuYawCur, imuRollCur, rx, ry, rz);
        }



        geometry_msgs::Quaternion geoQuat = tf::createQuaternionMsgFromRollPitchYaw(rz, -rx, -ry);

        laserOdometry.pose.pose.orientation.x = -geoQuat.y;
        laserOdometry.pose.pose.orientation.y = -geoQuat.z;
        laserOdometry.pose.pose.orientation.z = geoQuat.x;
        laserOdometry.pose.pose.orientation.w = geoQuat.w;
        laserOdometry.pose.pose.position.x = tx;
        laserOdometry.pose.pose.position.y = ty;
        laserOdometry.pose.pose.position.z = tz;

        pubOdo = laserOdometry;
        //pubLaserOdometry->publish(laserOdometry);
        ROS_WARN ("PUBLISHED ODOMETRY tx=%f, ty=%f, tz=%f, qx=%f, qy=%f, qz=%f, qw=%f",tx,ty,tz,geoQuat.x,geoQuat.y,geoQuat.z,geoQuat.w);


        laserOdometryTrans->setRotation(tf::Quaternion(-geoQuat.y, -geoQuat.z, geoQuat.x, geoQuat.w));
        laserOdometryTrans->setOrigin(tf::Vector3(tx, ty, tz));
        //tfBroadcaster->sendTransform(*laserOdometryTrans);

    }
}

void laserOdometry::setTransformationMatrix(double rx, double ry, double rz, double tx, double ty, double tz)
{
    ROS_WARN ("RESULT tx=%f, ty=%f, tz=%f, rx=%f, ry=%f, rz=%f",tx,ty,tz,rx,ry,rz);
    Eigen::Quaterniond q;
    geometry_msgs::Quaternion geoQuat = tf::createQuaternionMsgFromRollPitchYaw(rz, ry, rx);
    q.x() = geoQuat.x;
    q.y() = geoQuat.y;
    q.z() = geoQuat.z;
    q.w() = geoQuat.w;
    Eigen::Matrix3d R = q.matrix();
    T_transform << R(0,0), R(0,1), R(0,2), tx,
            R(1,0), R(1,1), R(1,2), ty,
            R(2,0), R(2,1), R(2,2), tz,
            0, 0, 0, 1;
}

int laserOdometry::processEdgePoint(pcl::KdTreeFLANN<pcl::PointXYZHSV>::Ptr kdtreeSurfPtr,  pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudSurfPtr, bool isPointSel, int laserCloudSurfNum, int i, int iterCount, int iterNum)
{
    int closestPointInd = -1, minPointInd2 = -1, minPointInd3 = -1;
    if (isPointSel)
    {
        kdtreeSurfPtr->nearestKSearch(extreSel, 1, pointSearchInd, pointSearchSqDis);
        if (pointSearchSqDis[0] > 10.0f) {
            return -1;
        }

        closestPointInd = pointSearchInd[0];
        //        float closestPointTime = laserCloudSurfPtr->points[closestPointInd].h;
        float pointSqDis, minPointSqDis2 = 1, minPointSqDis3 = 1;
        for (int j = closestPointInd + 1; j < laserCloudSurfNum; j++) {
            //            if (laserCloudSurfPtr->points[j].h > closestPointTime + 0.07) {
            //                break;
            //            }

            pointSqDis = SQRDIST(laserCloudSurfPtr->points[j].x,laserCloudSurfPtr->points[j].y,laserCloudSurfPtr->points[j].z,extreSel.x,extreSel.y,extreSel.z);

            //            if (laserCloudSurfPtr->points[j].h < closestPointTime + 0.005) {
            if (pointSqDis < minPointSqDis2) {
                minPointSqDis2 = pointSqDis;
                minPointInd2 = j;
            }
            //            }
            //            else
            //            {
            //                if (pointSqDis < minPointSqDis3) {
            //                    minPointSqDis3 = pointSqDis;
            //                    minPointInd3 = j;
            //                }
            //            }
        }
        for (int j = closestPointInd - 1; j >= 0; j--) {
            //            if (laserCloudSurfPtr->points[j].h < closestPointTime - 0.07) {
            //                break;
            //            }

            pointSqDis = SQRDIST(laserCloudSurfPtr->points[j].x,laserCloudSurfPtr->points[j].y,laserCloudSurfPtr->points[j].z,extreSel.x,extreSel.y,extreSel.z);

            //            if (laserCloudSurfPtr->points[j].h > closestPointTime - 0.005) {
            //            if (pointSqDis < minPointSqDis2) {
            //                minPointSqDis2 = pointSqDis;
            //                minPointInd2 = j;
            //            }
            //            }
            //            else
            //            {
            if (pointSqDis < minPointSqDis3) {
                minPointSqDis3 = pointSqDis;
                minPointInd3 = j;
            }
            //            }
        }
    } else {
        if (pointSelInd[3 * i] >= 0) {
            closestPointInd = pointSelInd[3 * i];
            minPointInd2 = pointSelInd[3 * i + 1];
            minPointInd3 = pointSelInd[3 * i + 2];

            float dist = SQRDIST(extreSel.x,extreSel.y,extreSel.z,laserCloudSurfPtr->points[closestPointInd].x,laserCloudSurfPtr->points[closestPointInd].y,laserCloudSurfPtr->points[closestPointInd].z);
            if (dist > 10.0f) {
                return -2;
            }
        } else {
            return -3;
        }
    }

    if (minPointInd2 >= 0 && minPointInd3 >= 0)
    {
        tripod1 = laserCloudSurfPtr->points[closestPointInd];
        tripod2 = laserCloudSurfPtr->points[minPointInd2];
        tripod3 = laserCloudSurfPtr->points[minPointInd3];

        float pa = (tripod2.y - tripod1.y) * (tripod3.z - tripod1.z) - (tripod3.y - tripod1.y) * (tripod2.z - tripod1.z);
        float pb = (tripod2.z - tripod1.z) * (tripod3.x - tripod1.x) - (tripod3.z - tripod1.z) * (tripod2.x - tripod1.x);
        float pc = (tripod2.x - tripod1.x) * (tripod3.y - tripod1.y) - (tripod3.x - tripod1.x) * (tripod2.y - tripod1.y);
        float pd = -(pa * tripod1.x + pb * tripod1.y + pc * tripod1.z);

        float ps = sqrt(pa * pa + pb * pb + pc * pc);
        pa /= ps;
        pb /= ps;
        pc /= ps;
        pd /= ps;

        float pd2 = pa * extreSel.x + pb * extreSel.y + pc * extreSel.z + pd;

        extreProj = extreSel;
        extreProj.x -= pa * pd2;
        extreProj.y -= pb * pd2;
        extreProj.z -= pc * pd2;

        float s = 1;
        if (iterCount >= 30) {
            s = 1 - 8 * fabs(pd2) / sqrt(sqrt(extreSel.x * extreSel.x + extreSel.y * extreSel.y + extreSel.z * extreSel.z));
        }

        coeff.x = s * pa;
        coeff.y = s * pb;
        coeff.z = s * pc;
        coeff.h = s * pd2;

        if (s > 0.2 || iterNum < 30) {
            laserCloudExtreOri->push_back(extreOri);
            number_edge++;
            //laserCloudExtreSel->push_back(extreSel);
            //laserCloudExtreProj->push_back(extreProj);
            //laserCloudSel->push_back(tripod1);
            //laserCloudSel->push_back(tripod2);
            //laserCloudSel->push_back(tripod3);
            if (std::isnan(coeff.x) || std::isnan(coeff.y) || std::isnan(coeff.z) || std::isnan(coeff.h))
            {
                std::cout << "s=" << s
                          << ", pa=" << pa
                          << ", pb=" << pb
                          << ", pd2=" << pd2 << std::endl;
            }
            coeffSel->push_back(coeff);

            if (isPointSel) {
                pointSelInd[3 * i] = closestPointInd;
                pointSelInd[3 * i + 1] = minPointInd2;
                pointSelInd[3 * i + 2] = minPointInd3;
            }
        } else {
            //std::cout << "s=" << s << std::endl;
            //laserCloudExtreUnsel->push_back(extreSel);
        }
    }
    else
    {
        return -4;
    }
}

int laserOdometry::processPlanarPoint(pcl::KdTreeFLANN<pcl::PointXYZHSV>::Ptr kdtreeCornerPtr, pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudCornerPtr, bool isPointSel, int laserCloudCornerNum, int i)
{
    int closestPointInd = -1, minPointInd2 = -1;
    if (isPointSel) {
        kdtreeCornerPtr->nearestKSearch(extreSel, 1, pointSearchInd, pointSearchSqDis);
        if (pointSearchSqDis[0] > 10.0) {
            return -1;
        }

        closestPointInd = pointSearchInd[0];
        //float closestPointTime = laserCloudCornerPtr->points[closestPointInd].h;
        float pointSqDis, minPointSqDis2 = 1;
        for (int j = closestPointInd + 1; j < laserCloudCornerNum; j++) {
            //            if (laserCloudCornerPtr->points[j].h > closestPointTime + 0.07) {
            //                break;
            //            }

            pointSqDis = SQRDIST(laserCloudCornerPtr->points[j].x,laserCloudCornerPtr->points[j].y,laserCloudCornerPtr->points[j].z, extreSel.x, extreSel.y, extreSel.z);

            //  if (laserCloudCornerPtr->points[j].h > closestPointTime + 0.005){
            if (pointSqDis < minPointSqDis2) {
                minPointSqDis2 = pointSqDis;
                minPointInd2 = j;
            }
            //}
        }

        for (int j = closestPointInd - 1; j >= 0; j--) {
            //            if (laserCloudCornerPtr->points[j].h < closestPointTime - 0.07) {
            //                break;
            //            }

            pointSqDis = SQRDIST(laserCloudCornerPtr->points[j].x,laserCloudCornerPtr->points[j].y,laserCloudCornerPtr->points[j].z,extreSel.x,extreSel.y,extreSel.z);

            //   if (laserCloudCornerPtr->points[j].h < closestPointTime - 0.005) {
            if (pointSqDis < minPointSqDis2) {
                minPointSqDis2 = pointSqDis;
                minPointInd2 = j;
            }
            //  }
        }
    } else {
        if (pointSelInd[3 * i] >= 0) {
            closestPointInd = pointSelInd[3 * i];
            minPointInd2 = pointSelInd[3 * i + 1];

            float dist = SQRDIST(extreSel.x,extreSel.y,extreSel.z,laserCloudCornerPtr->points[closestPointInd].x,laserCloudCornerPtr->points[closestPointInd].y,laserCloudCornerPtr->points[closestPointInd].z);
            if (dist > 10.0f) {
                return -2;
            }
        } else {
            return -3;
        }
    }

    if (minPointInd2 >= 0)
    {
        tripod1 = laserCloudCornerPtr->points[closestPointInd];
        tripod2 = laserCloudCornerPtr->points[minPointInd2];

        float x0 = extreSel.x;
        float y0 = extreSel.y;
        float z0 = extreSel.z;
        float x1 = tripod1.x;
        float y1 = tripod1.y;
        float z1 = tripod1.z;
        float x2 = tripod2.x;
        float y2 = tripod2.y;
        float z2 = tripod2.z;

        float a012 = sqrt(((x0 - x1)*(y0 - y2) - (x0 - x2)*(y0 - y1))
                          * ((x0 - x1)*(y0 - y2) - (x0 - x2)*(y0 - y1))
                          + ((x0 - x1)*(z0 - z2) - (x0 - x2)*(z0 - z1))
                          * ((x0 - x1)*(z0 - z2) - (x0 - x2)*(z0 - z1))
                          + ((y0 - y1)*(z0 - z2) - (y0 - y2)*(z0 - z1))
                          * ((y0 - y1)*(z0 - z2) - (y0 - y2)*(z0 - z1)));
        if (a012>0.001f)
        {
            if (std::isnan(a012) || std::isnan(-a012))
                std::cerr << "a012=" << a012 << std::endl;

            float l12 = sqrt((x1 - x2)*(x1 - x2) + (y1 - y2)*(y1 - y2) + (z1 - z2)*(z1 - z2));
            if (std::isnan(l12) || std::isnan(-l12))
                std::cerr << "l12=" << l12 << std::endl;
            float la = ((y1 - y2)*((x0 - x1)*(y0 - y2) - (x0 - x2)*(y0 - y1)) + (z1 - z2)*((x0 - x1)*(z0 - z2) - (x0 - x2)*(z0 - z1))) / a012 / l12;
            if (std::isnan(la) || std::isnan(-la))
                std::cerr << "la=" << la << ", a012=" << a012 << ", l12=" << l12 << std::endl;
            float lb = -((x1 - x2)*((x0 - x1)*(y0 - y2) - (x0 - x2)*(y0 - y1))  - (z1 - z2)*((y0 - y1)*(z0 - z2) - (y0 - y2)*(z0 - z1))) / a012 / l12;
            float lc = -((x1 - x2)*((x0 - x1)*(z0 - z2) - (x0 - x2)*(z0 - z1))  + (y1 - y2)*((y0 - y1)*(z0 - z2) - (y0 - y2)*(z0 - z1))) / a012 / l12;
            float ld2 = a012 / l12;

            extreProj = extreSel;
            extreProj.x -= la * ld2;
            extreProj.y -= lb * ld2;
            extreProj.z -= lc * ld2;

            float s = 2 * (1 - 8 * fabs(ld2));

            coeff.x = s * la;
            coeff.y = s * lb;
            coeff.z = s * lc;
            coeff.h = s * ld2;

            if (s > 0.4)
            {
                laserCloudExtreOri->push_back(extreOri);
                number_planar++;
                //laserCloudExtreSel->push_back(extreSel);
                //laserCloudExtreProj->push_back(extreProj);
                //laserCloudSel->push_back(tripod1);
                //laserCloudSel->push_back(tripod2);
                if (std::isnan(coeff.x) || std::isnan(coeff.y) || std::isnan(coeff.z) || std::isnan(coeff.h))
                {
                    std::cout << "s=" << s
                              << ", la=" << la
                              << ", lb=" << lb
                              << ", lc=" << lc
                              << ", ld2="<< ld2 << std::endl;
                }
                coeffSel->push_back(coeff);

                if (isPointSel) {
                    pointSelInd[3 * i] = closestPointInd;
                    pointSelInd[3 * i + 1] = minPointInd2;
                }
            } else {
                //laserCloudExtreUnsel->push_back(extreSel);
                //std::cout << "s=" << s << std::endl;
            }
        }
    }
    else
    {
        // std::cout << "minPointInd2)" << minPointInd2 << " >= 0" << std::endl;
    }
}

}
