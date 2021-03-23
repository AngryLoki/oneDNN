.. SPDX-FileCopyrightText: 2020 Intel Corporation
..
.. SPDX-License-Identifier: CC-BY-4.0

--------
Subtract
--------

**Versioned name**: *Subtract-1*

**Category**: *Arithmetic*

**Short description**: *Subtract* performs element-wise subtraction operation with two
given tensors applying multi-directional broadcast rules.

**OpenVINO description**: This OP is as same as `OpenVINO OP
<https://docs.openvinotoolkit.org/latest/openvino_docs_ops_arithmetic_Subtract_1.html>`__

**Attributes**:

* *auto_broadcast*

  * **Description**: specifies rules used for auto-broadcasting of input
    tensors.
  * **Range of values**:

    * *none* - no auto-broadcasting is allowed, all input shapes should match
    * *numpy* - numpy broadcasting rules, aligned with ONNX Broadcasting.
      Description is available in `ONNX docs
      <https://github.com/onnx/onnx/blob/master/docs/Broadcasting.md>`__.

  * **Type**: string
  * **Default value**: "numpy"
  * **Required**: *no*

**Inputs**

* **1**: A tensor of type T. **Required.**
* **2**: A tensor of type T. **Required.**

**Outputs**

* **1**: The result of element-wise subtraction operation. A tensor of type T.

**Types**

* *T*: any numeric type.

**Detailed description**:

Before performing arithmetic operation, input tensors *a* and *b* are
broadcast if their shapes are different and ``auto_broadcast`` attributes is
not ``none``. Broadcasting is performed according to ``auto_broadcast`` value.

After broadcasting *Subtract* does the following with the input tensors *a* and *b*:

.. math::
   o_{i} = a_{i} - b_{i}
