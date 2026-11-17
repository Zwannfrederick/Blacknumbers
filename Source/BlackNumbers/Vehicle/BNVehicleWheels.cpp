#include "Vehicle/BNVehicleWheels.h"

UBNWheelFront::UBNWheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
	WheelRadius = 39.f;
	WheelWidth = 35.f;
	FrictionForceMultiplier = 3.f;
	MaxBrakeTorque = 4500.f;
	MaxHandBrakeTorque = 6000.f;
	bABSEnabled = true;
}

UBNWheelRear::UBNWheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedByEngine = true;
	bAffectedByHandbrake = true;
	MaxSteerAngle = 0.f;
	WheelRadius = 40.f;
	WheelWidth = 40.f;
	FrictionForceMultiplier = 4.f;
	SlipThreshold = 100.f;
	SkidThreshold = 100.f;
	MaxHandBrakeTorque = 6000.f;
	bABSEnabled = true;
	bTractionControlEnabled = true;
}
