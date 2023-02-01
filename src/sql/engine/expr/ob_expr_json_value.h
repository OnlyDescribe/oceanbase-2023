/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

// This file is for func json_value.
#ifndef OCEANBASE_SQL_OB_EXPR_JSON_VALUE_H_
#define OCEANBASE_SQL_OB_EXPR_JSON_VALUE_H_

#include "sql/engine/expr/ob_expr_operator.h"
#include "lib/json_type/ob_json_tree.h"
#include "lib/json_type/ob_json_base.h"

using namespace oceanbase::common;

namespace oceanbase
{
namespace sql
{
class 
ObExprJsonValue : public ObFuncExprOperator
{
public:
  explicit ObExprJsonValue(common::ObIAllocator &alloc);
  virtual ~ObExprJsonValue();
  virtual int calc_result_typeN(ObExprResType& type,
                                ObExprResType* types,
                                int64_t param_num, 
                                common::ObExprTypeCtx& type_ctx)
                                const override;
  virtual int calc_resultN(common::ObObj& result,
                           const common::ObObj* objs_stack,
                           int64_t param_num,
                           common::ObExprCtx& expr_ctx)
                           const override;
  static int eval_json_value(const ObExpr &expr, ObEvalCtx &ctx, ObDatum &res);
  virtual int cg_expr(ObExprCGCtx &expr_cg_ctx, const ObRawExpr &raw_expr,
                      ObExpr &rt_expr) const override;
  virtual common::ObCastMode get_cast_mode() const { return CM_ERROR_ON_SCALE_OVER;}
private:
  /* code for cast accuracy check */
  template<typename Obj>
  static int check_default_val_accuracy(const ObAccuracy &accuracy,
                                        const ObObjType &type,
                                        const Obj *obj);
  static int get_accuracy_internal(common::ObAccuracy &accuracy,
                                  ObObjType &dest_type,
                                  const int64_t value,
                                  const ObLengthSemantics &length_semantics);
  static int get_accuracy(const ObExpr &expr,
                          ObEvalCtx& ctx,
                          common::ObAccuracy &accuracy,
                          ObObjType &dest_type,
                          bool &is_cover_by_error);
  static int number_range_check(const common::ObAccuracy &accuracy,
                                ObIAllocator *allocator,
                                number::ObNumber &val,
                                bool strict = false);
  static int datetime_scale_check(const common::ObAccuracy &accuracy,
                                  int64_t &value,
                                  bool strict = false);
  static int time_scale_check(const common::ObAccuracy &accuracy, int64_t &value,
                              bool strict = false);
  /* cast wrapper to dst type with accuracy check*/
  static int get_cast_ret(int ret);
  static int cast_to_int(ObIJsonBase *j_base, ObObjType dst_type, int64_t &val);
  static int cast_to_uint(ObIJsonBase *j_base, ObObjType dst_type, uint64_t &val);
  static int cast_to_datetime(ObIJsonBase *j_base,
                              common::ObAccuracy &accuracy,
                              int64_t &val);
  static int cast_to_otimstamp(ObIJsonBase *j_base,
                               const ObBasicSessionInfo *session,
                               common::ObAccuracy &accuracy,
                               ObObjType dst_type,
                               ObOTimestampData &out_val);
  static int cast_to_date(ObIJsonBase *j_base, int32_t &val);
  static int cast_to_time(ObIJsonBase *j_base,
                          common::ObAccuracy &accuracy,
                          int64_t &val);
  static int cast_to_year(ObIJsonBase *j_base, uint8_t &val);
  static int cast_to_float(ObIJsonBase *j_base, ObObjType dst_type, float &val);
  static int cast_to_double(ObIJsonBase *j_base, ObObjType dst_type, double &val);
  static int cast_to_number(common::ObIAllocator *allocator,
                            ObIJsonBase *j_base,
                            common::ObAccuracy &accuracy,
                            ObObjType dst_type,
                            number::ObNumber &val);
  static int cast_to_string(common::ObIAllocator *allocator,
                            ObIJsonBase *j_base,
                            ObCollationType in_cs_type,
                            ObCollationType dst_cs_type,
                            common::ObAccuracy &accuracy,
                            ObObjType dst_type,
                            ObString &val);
  static int cast_to_bit(ObIJsonBase *j_base, uint64_t &val);
  static int cast_to_json(common::ObIAllocator *allocator, ObIJsonBase *j_base, ObString &val);
  static int cast_to_res(common::ObIAllocator *allocator,
                         ObExprCtx& expr_ctx,
                         ObIJsonBase *j_base,
                         uint8_t error_type,
                         ObObj *error_val,
                         common::ObAccuracy &accuracy,
                         ObObjType dst_type,
                         common::ObCollationType in_coll_type,
                         common::ObCollationType dst_coll_type,
                         common::ObCollationLevel dst_coll_level,
                         ObObj &res);
  static int cast_to_res(common::ObIAllocator *allocator,
                         const ObExpr &expr,
                         ObEvalCtx &ctx,
                         ObIJsonBase *j_base,
                         uint8_t error_type,
                         ObDatum *error_val,
                         common::ObAccuracy &accuracy,
                         ObObjType dst_type,
                         common::ObCollationType in_coll_type,
                         common::ObCollationType dst_coll_type,
                         ObDatum &res);
  template<typename Obj>
  static bool try_set_error_val(Obj &res, int &ret, uint8_t error_type, Obj *error_val, const char *type = NULL);
  // new sql engine
  static inline void set_val(ObDatum &res, ObDatum *val)
  { res.set_datum(*val); }

  // old sql engine
  static inline void set_val(ObObj &res, ObObj *val)
  { res = *val; }

  /* process empty or error */
  const static uint8_t OB_JSON_ON_RESPONSE_ERROR    = 0;
  const static uint8_t OB_JSON_ON_RESPONSE_NULL     = 1;
  const static uint8_t OB_JSON_ON_RESPONSE_DEFAULT  = 2;
  const static uint8_t OB_JSON_ON_RESPONSE_IMPLICIT = 3;
  int get_on_empty_or_error_old(const ObObj *params,
                                ObExprCtx& expr_ctx,
                                const ObObjType dst_type,
                                uint8_t index,
                                bool &is_cover_by_error,
                                const ObAccuracy &accuracy,
                                uint8 &type,
                                ObObj *default_value) const;
  static int get_on_empty_or_error(const ObExpr &expr,
                                   ObEvalCtx &ctx,
                                   uint8_t index,
                                   bool &is_cover_by_error,
                                   const ObAccuracy &accuracy,
                                   uint8 &type,
                                   ObDatum **default_value);
  /* code from ob_expr_cast for cal_result_type */
  const static int32_t OB_LITERAL_MAX_INT_LEN = 21;
  int get_cast_type(const ObExprResType param_type2, ObExprResType &dst_type) const;
  int set_dest_type(ObExprResType &type1, ObExprResType &type, ObExprResType &dst_type, ObExprTypeCtx &type_ctx) const;
  int get_cast_string_len(ObExprResType &type1,
                          ObExprResType &type2,
                          common::ObExprTypeCtx &type_ctx,
                          int32_t &res_len,
                          int16_t &length_semantics,
                          common::ObCollationType conn) const;
  int get_cast_inttc_len(ObExprResType &type1,
                         ObExprResType &type2,
                         common::ObExprTypeCtx &type_ctx,
                         int32_t &res_len,
                         int16_t &length_semantics,
                         common::ObCollationType conn) const;
  // disallow copy
  DISALLOW_COPY_AND_ASSIGN(ObExprJsonValue);
};

} // sql
} // oceanbase
#endif // OCEANBASE_SQL_OB_EXPR_JSON_VALUE_H_
