/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <NvInfer.h>

#include <vector>
#include <cassert>

#include "../cuda/nms_iou.h"

using namespace nvinfer1;

#define RETINANET_PLUGIN_NAME "RetinaNetNMSRotate"
#define RETINANET_PLUGIN_VERSION "1"
#define RETINANET_PLUGIN_NAMESPACE ""

namespace retinanet {

class NMSRotatePlugin : public IPluginV2Ext {
  float _nms_thresh;
  int _detections_per_im;

  size_t _count;

  mutable int size = -1;

protected:
  void deserialize(void const* data, size_t length) {
    const char* d = static_cast<const char*>(data);
    read(d, _nms_thresh);
    read(d, _detections_per_im);
    read(d, _count);
  }

  size_t getSerializationSize() const override {
    return sizeof(_nms_thresh) + sizeof(_detections_per_im)
      + sizeof(_count);
  }

  void serialize(void *buffer) const override {
    char* d = static_cast<char*>(buffer);
    write(d, _nms_thresh);
    write(d, _detections_per_im);
    write(d, _count);
  }

public:
  NMSRotatePlugin(float nms_thresh, int detections_per_im)
    : _nms_thresh(nms_thresh), _detections_per_im(detections_per_im) {
    assert(nms_thresh > 0);
    assert(detections_per_im > 0);
  }

  NMSRotatePlugin(float nms_thresh, int detections_per_im, size_t count)
    : _nms_thresh(nms_thresh), _detections_per_im(detections_per_im), _count(count) {
    assert(nms_thresh > 0);
    assert(detections_per_im > 0);
    assert(count > 0);
  }

  NMSRotatePlugin(void const* data, size_t length) {
    this->deserialize(data, length);
  }

  const char *getPluginType() const override {
    return RETINANET_PLUGIN_NAME;
  }
 
  const char *getPluginVersion() const override {
    return RETINANET_PLUGIN_VERSION;
  }
  
  int getNbOutputs() const override {
    return 3;
  }

  Dims getOutputDimensions(int index,
                                     const Dims *inputs, int nbInputDims) override {
    assert(nbInputDims == 3);
    assert(index < this->getNbOutputs());
    return Dims3(_detections_per_im * (index == 1 ? 6 : 1), 1, 1);
  }

  bool supportsFormat(DataType type, PluginFormat format) const override {
    return type == DataType::kFLOAT && format == PluginFormat::kLINEAR;
  }

  int initialize() override { return 0; }

  void terminate() override {}

  size_t getWorkspaceSize(int maxBatchSize) const override {
    if (size < 0) {
      size = cuda::nms_rotate(maxBatchSize, nullptr, nullptr, _count, 
        _detections_per_im, _nms_thresh, 
        nullptr, 0, nullptr);
    }
    return size;
  }

  int enqueue(int batchSize,
              const void *const *inputs, void **outputs,
              void *workspace, cudaStream_t stream) override {
    return cuda::nms_rotate(batchSize, inputs, outputs, _count, 
      _detections_per_im, _nms_thresh,
      workspace, getWorkspaceSize(batchSize), stream);
  }

  void destroy() override {
    delete this;
  }

  const char *getPluginNamespace() const override {
    return RETINANET_PLUGIN_NAMESPACE;
  }
  
  void setPluginNamespace(const char *N) override {
    
  }

  // IPluginV2Ext Methods
  DataType getOutputDataType(int index, const DataType* inputTypes, int nbInputs) const
  {
    assert(index < 3);
    return DataType::kFLOAT;
  }

  bool isOutputBroadcastAcrossBatch(int outputIndex, const bool* inputIsBroadcasted, 
    int nbInputs) const { return false; }

  bool canBroadcastInputAcrossBatch(int inputIndex) const { return false; }

  void configurePlugin(const Dims* inputDims, int nbInputs, const Dims* outputDims, int nbOutputs,
    const DataType* inputTypes, const DataType* outputTypes, const bool* inputIsBroadcast,
    const bool* outputIsBroadcast, PluginFormat floatFormat, int maxBatchSize)
  {
    assert(*inputTypes == nvinfer1::DataType::kFLOAT &&
      floatFormat == nvinfer1::PluginFormat::kLINEAR);
    assert(nbInputs == 3);
    assert(inputDims[0].d[0] == inputDims[2].d[0]);
    assert(inputDims[1].d[0] == inputDims[2].d[0] * 6);
    _count = inputDims[0].d[0];
  }

  IPluginV2Ext *clone() const override {
    return new NMSRotatePlugin(_nms_thresh, _detections_per_im, _count);
  }

private:
  template<typename T> void write(char*& buffer, const T& val) const {
    *reinterpret_cast<T*>(buffer) = val;
    buffer += sizeof(T);
  }

  template<typename T> void read(const char*& buffer, T& val) {
    val = *reinterpret_cast<const T*>(buffer);
    buffer += sizeof(T);
  }
};

class NMSRotatePluginCreator : public IPluginCreator {
public:
  NMSRotatePluginCreator() {}
  
  const char *getPluginNamespace() const override {
    return RETINANET_PLUGIN_NAMESPACE;
  }
  const char *getPluginName () const override {
    return RETINANET_PLUGIN_NAME;
  }

  const char *getPluginVersion () const override {
    return RETINANET_PLUGIN_VERSION;
  }
 
  IPluginV2 *deserializePlugin (const char *name, const void *serialData, size_t serialLength) override {
    return new NMSRotatePlugin(serialData, serialLength);
  }

  void setPluginNamespace(const char *N) override {}
  const PluginFieldCollection *getFieldNames() override { return nullptr; }
  IPluginV2 *createPlugin (const char *name, const PluginFieldCollection *fc) override { return nullptr; }
};

REGISTER_TENSORRT_PLUGIN(NMSRotatePluginCreator);

}

#undef RETINANET_PLUGIN_NAME
#undef RETINANET_PLUGIN_VERSION
#undef RETINANET_PLUGIN_NAMESPACE
