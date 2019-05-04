/* Copyright © 2019 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef TURI_ONE_SHOT_OBJECT_DETECTOR_H_
#define TURI_ONE_SHOT_OBJECT_DETECTOR_H_

#include <map>
#include <unity/lib/extensions/ml_model.hpp>
#include <unity/lib/gl_sframe.hpp>
#include <unity/toolkits/coreml_export/mlmodel_wrapper.hpp>
#include <unity/toolkits/object_detection/object_detector.hpp>

namespace turi {
namespace one_shot_object_detection {

class EXPORT one_shot_object_detector: public ml_model_base {

 public:

  // Constructor
  one_shot_object_detector();

  // Interface exposed via Unity server

  // TODO: augment -> train
  gl_sframe augment(gl_sframe data,
                    const std::string& target_column_name,
                    gl_sarray backgrounds,
                    std::map<std::string, flexible_type> options);

  BEGIN_CLASS_MEMBER_REGISTRATION("one_shot_object_detector")

  IMPORT_BASE_CLASS_REGISTRATION(ml_model_base);

  REGISTER_CLASS_MEMBER_FUNCTION(one_shot_object_detector::augment, "data",
                                 "target_column_name", "backgrounds", "options");

  END_CLASS_MEMBER_REGISTRATION

 private:
  // Obsolete until we actually use the object_detector::train.
  // Leaving it here anyway unless we decide we should remove it.
  std::unique_ptr<turi::object_detection::object_detector> model_;

};

} // one_shot_object_detection
} // turi

#endif