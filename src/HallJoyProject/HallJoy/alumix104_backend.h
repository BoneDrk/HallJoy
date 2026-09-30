#pragma once
#include "native_analog_backend.h"

const NativeAnalogBackendDescriptor& Alumix104_GetNativeBackendDescriptor();
// Aggregate output evidence for the temporary gamepad trial. A publication
// while connected is not by itself proof that Alumix caused that output.
bool Alumix104_TrialConnected() noexcept;
void Alumix104_ObserveGamepadCandidate(unsigned nonneutralPadMask, bool changed,
                                       bool publishDue) noexcept;
void Alumix104_ObserveGamepadPublish(bool published, bool nonneutral) noexcept;
