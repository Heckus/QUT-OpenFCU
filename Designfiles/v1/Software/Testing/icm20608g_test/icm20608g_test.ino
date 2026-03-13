#include "ICM-20608-G.h"

// see the header files for available ranges
ICM_20608_G imu(GyroRange::G_500_DEGS, AccelRange::A_4_G);

void setup()
{
    SPI.begin(); // start the SPI
    delay(10);
    imu.init();
    //imu.calibrate(); // only do this if your z-axis points upwards

    Serial.begin(9600);
}

void loop()
{
    float gx, gy, gz, ax, ay, az;

    imu.readGyro(gx, gy, gz);
    imu.readAccel(ax, ay, az);

    // do something
    Serial.println();
    Serial.print("GYRO X: ");
    Serial.print(gx);
    Serial.print("\tGYRO Y: ");
    Serial.print(gy);
    Serial.print("\tGYRO Z: ");
    Serial.print(gz);
    Serial.println("[deg/s]");

    Serial.print("ACCEL X: ");
    Serial.print(ax);
    Serial.print("\tACCEL Y: ");
    Serial.print(ay);
    Serial.print("\tACCEL Z: ");
    Serial.print(az);
    Serial.println("[g]");
    Serial.println();

    delay(100);
}
