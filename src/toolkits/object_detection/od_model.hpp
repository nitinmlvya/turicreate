/* Copyright © 2020 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at
 * https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef TOOLKITS_OBJECT_DETECTION_OD_MODEL_HPP_
#define TOOLKITS_OBJECT_DETECTION_OD_MODEL_HPP_

/**
 * \file od_model.hpp
 *
 * Defines the value types representing each stage of an object-detection
 * training pipeline, and the virtual interface for arbitrary object-detection
 * models.
 */

#include <ml/neural_net/combine.hpp>
#include <toolkits/object_detection/od_data_iterator.hpp>

namespace turi {
namespace object_detection {

/** Represents one batch of raw data: (possibly) annotated images. */
struct DataBatch {
  /** The serial number for this batch, starting with 1. */
  int iteration_id = 0;

  std::vector<neural_net::labeled_image> examples;
};

/** Represents one batch of model-agnostic data, post-augmentation/resizing. */
struct InputBatch {
  int iteration_id = 0;

  // TODO: Adopt NCHW.
  /** The (RGB) images from a DataBatch encoded as NHWC. */
  neural_net::shared_float_array images;

  /** The raw annotations from the DataBatch. */
  std::vector<std::vector<neural_net::image_annotation>> annotations;
};

/** Represents one batch of data, in a possibly model-specific format. */
struct EncodedInputBatch {
  int iteration_id = 0;
  neural_net::shared_float_array images;
  neural_net::shared_float_array labels;

  // The raw annotations are preserved to support evaluation, comparing raw
  // annotations against model predictions.
  std::vector<std::vector<neural_net::image_annotation>> annotations;
};

/** Represents the raw output of an object-detection model. */
struct TrainingOutputBatch {
  int iteration_id = 0;
  neural_net::shared_float_array loss;
};

/** Represents the output conveyed to the user. */
struct TrainingProgress {
  int iteration_id = 0;
  float smoothed_loss = 0.f;
};

/** Ostensibly model-agnostic parameters for object detection. */
struct Config {
  /**
   * The target number of training iterations to perform.
   *
   * If -1, then this target should be computed heuristically.
   */
  int max_iterations = -1;

  /**
   * The number of images to process per training batch.
   *
   * If -1, then this size should be computed automatically.
   */
  int batch_size = -1;

  /** For darknet-yolo, the height of the final feature map. */
  int output_height = 13;

  /** For darknet-yolo, the width of the final feature map. */
  int output_width = 13;

  /** Determines the number of feature channels in the final feature map. */
  int num_classes = -1;
};

/**
 * A representation of all the parameters needed to reconstruct a model.
 *
 * \todo Include optimizer state to allow training to resume seamlessly.
 */
struct Checkpoint {
  Config config;
  neural_net::float_array_map weights;
};

/**
 * Wrapper adapting object_detection::data_iterator to the Iterator interface.
 */
class DataIterator : public neural_net::Iterator<DataBatch> {
 public:
  /**
   * \param impl The object_detection::data_iterator to wrap
   * \param batch_size The number of images to request from impl for each batch.
   * \param offset The number of batches to skip. The first batch produced will
   *     have an iteration_id one more than the offset.
   *
   * \todo object_detection::data_iterator needs to support specifying the
   *     offset (and doing the right thing with random seeding)
   */
  DataIterator(std::unique_ptr<data_iterator> impl, size_t batch_size,
               int offset = 0)
      : impl_(std::move(impl)),
        batch_size_(batch_size),
        last_iteration_id_(offset) {}

  bool HasNext() const override { return impl_->has_next_batch(); }

  DataBatch Next() override;

 private:
  std::unique_ptr<data_iterator> impl_;
  size_t batch_size_ = 32;
  int last_iteration_id_ = 0;  // Next ID starts at 1, not 0, by default.
};

/** Wrapper adapting image_augmenter to the Transform interface. */
class DataAugmenter : public neural_net::Transform<DataBatch, InputBatch> {
 public:
  DataAugmenter(std::unique_ptr<neural_net::image_augmenter> impl)
      : impl_(std::move(impl)) {}

  InputBatch Invoke(DataBatch data_batch) override;

 private:
  std::unique_ptr<neural_net::image_augmenter> impl_;
};

/**
 * Converts raw training output to user-visible progress updates.
 *
 * \todo Adopt this operator once model_backend supports an async API that would
 * avoid performance regressions due to premature waiting on the futures that
 * the model_backend implementations currently output.
 */
class ProgressUpdater
    : public neural_net::Transform<TrainingOutputBatch, TrainingProgress> {
 public:
  ProgressUpdater(std::unique_ptr<float> smoothed_loss)
      : smoothed_loss_(std::move(smoothed_loss)) {}

  TrainingProgress Invoke(TrainingOutputBatch output_batch) override;

 private:
  std::unique_ptr<float> smoothed_loss_;
};

/**
 * Abstract base class for object-detection models.
 *
 * Responsible for constructing the model-agnostic portions of the overall
 * training pipeline.
 */
class Model {
 public:
  // TODO: This class should be responsible for producing the augmenter itself.
  Model(std::unique_ptr<neural_net::image_augmenter> augmenter);

  virtual ~Model() = default;

  /**
   * Given a data iterator, return a publisher of model outputs.
   *
   * \todo Eventually this should return a TrainingProgress publisher.
   */
  virtual std::shared_ptr<neural_net::Publisher<TrainingOutputBatch>>
  AsTrainingBatchPublisher(std::unique_ptr<data_iterator> training_data,
                           size_t batch_size, int offset);

  /** Returns a publisher that can be used to request checkpoints. */
  virtual std::shared_ptr<neural_net::Publisher<Checkpoint>>
  AsCheckpointPublisher() = 0;

 protected:
  // Used by subclasses to produce the model-specific portions of the overall
  // training pipeline.
  virtual std::shared_ptr<neural_net::Publisher<TrainingOutputBatch>>
  AsTrainingBatchPublisher(
      std::shared_ptr<neural_net::Publisher<InputBatch>> augmented_data) = 0;

 private:
  std::shared_ptr<DataAugmenter> augmenter_;
};

}  // namespace object_detection
}  // namespace turi

#endif  // TOOLKITS_OBJECT_DETECTION_OD_MODEL_HPP_
