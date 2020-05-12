#include "fully_connected_kernel.hpp"

#include "symmetric_quantization_utils.hpp"

namespace uTensor {
namespace TFLM {

void quantized_matrix_mult_kernel(Tensor& output, const Tensor& input,
                                  const Tensor& filter,
                                  int32_t output_multiplier, int output_shift,
                                  int32_t output_activation_min,
                                  int32_t output_activation_max) {
  int32_t input_offset =
      -input->get_quantization_params().get_zeroP_for_channel(0);
  int32_t filter_offset =
      -filter->get_quantization_params().get_zeroP_for_channel(0);
  int32_t output_offset =
      output->get_quantization_params().get_zeroP_for_channel(0);
  output_shift = -output_shift;
  // quantized_activation_min = data->output_activation_min;
  // quantized_activation_max = data->output_activation_max;

  const TensorShape& input_shape = input->get_shape();
  const TensorShape& filter_shape = filter->get_shape();
  TensorShape& output_shape = output->get_shape();

  const int filter_dim_count = filter_shape.num_dims();
  const int batches = output_shape[0];
  const int output_depth = output_shape[1];
  if (!(output_depth < filter_shape[filter_dim_count - 2])) {
    Context::get_default_context()->throwError(
        new InvalidMatrixMultIndicesError);
  }
  const int accum_depth = filter_shape[filter_dim_count - 1];
  for (int b = 0; b < batches; ++b) {
    for (int out_c = 0; out_c < output_depth; ++out_c) {
      int32_t acc = 0;
      for (int d = 0; d < accum_depth; ++d) {
        // TODO write this in tensor form
        int32_t input_val = static_cast<int8_t>(input(b, d, 0, 0));
        int32_t filter_val = static_cast<int8_t>(filter(d, out_c, 0, 0));
        acc += (filter_val + filter_offset) * (input_val + input_offset);
      }
      acc = MultiplyByQuantizedMultiplier(acc, output_multiplier, output_shift);
      acc += output_offset;
      acc = std::max(acc, output_activation_min);
      acc = std::min(acc, output_activation_max);
      // output_data[out_c + output_depth * b] = static_cast<int8_t>(acc);
      // output_data(out_c + output_depth * b) = static_cast<int8_t>(acc);
      output(b, out_c) = static_cast<int8_t>(acc);
    }
  }
}
void quantized_matrix_mult_kernel(Tensor& output, const Tensor& input,
                                  const Tensor& filter, const Tensor& bias,
                                  int32_t output_multiplier, int output_shift,
                                  int32_t output_activation_min,
                                  int32_t output_activation_max) {
  int32_t input_offset =
      -input->get_quantization_params().get_zeroP_for_channel(0);
  int32_t filter_offset =
      -filter->get_quantization_params().get_zeroP_for_channel(0);
  int32_t output_offset =
      output->get_quantization_params().get_zeroP_for_channel(0);
  output_shift = -output_shift;
  // quantized_activation_min = data->output_activation_min;
  // quantized_activation_max = data->output_activation_max;

  const TensorShape& input_shape = input->get_shape();
  const TensorShape& filter_shape = filter->get_shape();
  TensorShape& output_shape = output->get_shape();

  const int filter_dim_count = filter_shape.num_dims();
  const int batches = output_shape[0];
  const int output_depth = output_shape[1];
  if (!(output_depth < filter_shape[filter_dim_count - 2])) {
    Context::get_default_context()->throwError(
        new InvalidMatrixMultIndicesError);
  }
  const int accum_depth = filter_shape[0];
  for (int b = 0; b < batches; ++b) {
    for (int out_c = 0; out_c < output_depth; ++out_c) {
      int32_t acc = 0;
      for (int d = 0; d < accum_depth; ++d) {
        // TODO write this in tensor form
        int32_t input_val = static_cast<int8_t>(input(b, d, 0, 0));
        int32_t filter_val = static_cast<int8_t>(filter(d, out_c, 0, 0));
        acc += (filter_val + filter_offset) * (input_val + input_offset);
      }
      acc += static_cast<int32_t>(bias(out_c));
      acc = MultiplyByQuantizedMultiplier(acc, output_multiplier, output_shift);
      acc += output_offset;
      acc = std::max(acc, output_activation_min);
      acc = std::min(acc, output_activation_max);
      // output_data[out_c + output_depth * b] = static_cast<int8_t>(acc);
      // output_data(out_c + output_depth * b) = static_cast<int8_t>(acc);
      output(b, out_c, 0, 0) = static_cast<int8_t>(acc);
    }
  }
}

}  // namespace TFLM

void quantized_matrix_mult_kernel(Tensor& output, const Tensor& input,
                                  const Tensor& filter, const Tensor& bias,
                                  int32_t output_activation_min,
                                  int32_t output_activation_max) {
  const int32_t input_zp =
      input->get_quantization_params().get_zeroP_for_channel(0);
  const float input_scale =
      input->get_quantization_params().get_scale_for_channel(0);
  const int32_t filter_zp =
      filter->get_quantization_params().get_zeroP_for_channel(0);
  const float filter_scale =
      filter->get_quantization_params().get_scale_for_channel(0);
  const int32_t bias_zp =
      bias->get_quantization_params().get_zeroP_for_channel(0);
  const float bias_scale =
      bias->get_quantization_params().get_scale_for_channel(0);
  const int32_t output_zp =
      output->get_quantization_params().get_zeroP_for_channel(0);
  const float output_scale =
      output->get_quantization_params().get_scale_for_channel(0);

  const TensorShape& input_shape = input->get_shape();
  const TensorShape& filter_shape = filter->get_shape();
  const uint16_t num_batches = input_shape[0];
  const uint16_t accum_depth = input_shape[1];
  const uint16_t output_depth = filter_shape[1];
  auto AsFloat8 = [](int8_t value, const int32_t zp, const float scale) {
    return static_cast<float>((static_cast<int32_t>(value) - zp)) * scale;
  };
  auto AsFloat32 = [](int32_t value, const int32_t zp, const float scale) {
    return static_cast<float>(value - zp) * scale;
  };
  auto QuatizeOutput = [output_zp, output_scale](float value_f) {
    const int32_t minVal =
        static_cast<int32_t>(std::numeric_limits<int8_t>::min());
    const int32_t maxVal =
        static_cast<int32_t>(std::numeric_limits<int8_t>::max());
    int32_t unclamped =
        static_cast<int32_t>(std::round(value_f / output_scale)) + output_zp;
    int32_t clamped = std::min(std::max(unclamped, minVal), maxVal);
    return clamped;
  };
  for (int b = 0; b < num_batches; ++b) {
    for (int out_c = 0; out_c < output_depth; ++out_c) {
      float acc = 0;
      for (int d = 0; d < accum_depth; ++d) {
        float input_f = AsFloat8(static_cast<int8_t>(input(b, d, 0, 0)),
                                 input_zp, input_scale);
        float filter_f = AsFloat8(static_cast<int8_t>(filter(d, out_c, 0, 0)),
                                  filter_zp, filter_scale);
        acc += input_f * filter_f;
      }
      float bias_f =
          AsFloat32(static_cast<int32_t>(bias(out_c)), bias_zp, bias_scale);
      acc += bias_f;
      int32_t acc_quant = QuatizeOutput(acc);
      acc_quant = std::max(acc_quant, output_activation_min);
      acc_quant = std::min(acc_quant, output_activation_max);
      output(b, out_c, 0, 0) = static_cast<int8_t>(acc_quant);
    }
  }
}
}  // namespace uTensor
