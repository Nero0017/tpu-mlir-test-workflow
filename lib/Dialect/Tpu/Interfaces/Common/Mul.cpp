//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Dialect/Tpu/IR/TpuOps.h"
#include "tpu_mlir/Support/Dnnl/Dnnl.h"
#include "tpu_mlir/Support/Float16.h"
#include "tpu_mlir/Support/Module.h"

#include "tpu_mlir/Support/MathUtils.h"

LogicalResult tpu::MulOp::init(InferenceParameter &p) {
  auto binary = new Binary();
  auto lhs_shape = module::getShape(getInputs()[0]);
  auto rhs_shape = module::getShape(getInputs()[1]);

  (*binary)
      .hs(p.inputs[0], p.inputs[1], lhs_shape, rhs_shape)
      .dst(p.outputs[0], module::getShape(getOutput()))
      .do_relu(getDoRelu())
      .relu_limit(getReluLimit().convertToDouble())
      .algorithem(algorithm::binary_mul)
      .setup();
  p.handle = (void *)binary;
  return success();
}

void tpu::MulOp::deinit(InferenceParameter &p) {
  if (p.handle != nullptr) {
    auto binary = (Binary *)p.handle;
    delete binary;
    p.handle = nullptr;
  }
}

LogicalResult tpu::MulOp::inference(InferenceParameter &p) {
  auto num_elem = module::getNumElements(getOutput());
  auto out_type = module::getStorageType(getOutput());
  auto asym = module::isAsymmetric();
  auto binary = (Binary *)p.handle;
  binary->run();
  if (out_type.isa<FloatType>()) {
    if (out_type.isBF16()) {
      BF16(p.outputs[0], p.outputs[0], num_elem);
    } else if (out_type.isF16()) {
      F16(p.outputs[0], p.outputs[0], num_elem);
    }
  } else if (out_type.isInteger(32)) {
    return success();
  } else if (asym == false) {
    auto qmode = getQuantMode();
#pragma omp parallel for schedule(static, omp_schedule(num_elem))
    for (int i = 0; i < num_elem; i++) {
      float sum = p.outputs[0][i];
      if (module::isCV18xx()) {
        sum = applyMultiplierAndRShift(sum, getMultiplier(), getRshift(), qmode,
                                       ROUNDING_HALF_AWAY_FROM_ZERO);
      } else {
        sum = applyMultiplierAndRShift(sum, getMultiplier(), getRshift(), qmode);
      }
      p.outputs[0][i] = saturate(sum, out_type);
    }
  } else {
    llvm_unreachable("MulOp asymmetric use FP32");
  }
  return success();
}

LogicalResult tpu::MulOp::LocalGenSupport() {
  // BackwardH and BackwardN can not handle more than one input right now.
  // The same n_slice and h_slice value will propagate to each inputs.
  // Thus, the local layer is only safe when we do not need to slice n and h
  // dimensions.
  auto out_shape = module::getShape(getOutput());
  auto lhs_shape = module::getShape(getInputs()[0]);
  auto rhs_shape = module::getShape(getInputs()[1]);
  if (lhs_shape.size() != rhs_shape.size())
    return failure();
  if (module::isWeight(getOperand(0)) || module::isWeight(getOperand(1)))
    return failure();
  // left align
  switch (out_shape.size()) {
  case 2:
    if (lhs_shape[0] != rhs_shape[0])
      return failure();
    return success();
  case 3:
  case 4:
    if (lhs_shape[0] != rhs_shape[0])
      return failure();
    if (lhs_shape[2] != rhs_shape[2])
      return failure();
  default:
    success();
  }
  return success();
}
