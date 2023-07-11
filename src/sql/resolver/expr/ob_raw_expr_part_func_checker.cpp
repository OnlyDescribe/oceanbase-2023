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

#define USING_LOG_PREFIX SQL_RESV
#include "sql/resolver/expr/ob_raw_expr_part_func_checker.h"

namespace oceanbase {
using namespace common;
namespace sql {

int ObRawExprPartFuncChecker::visit(ObConstRawExpr& expr)
{
  int ret = OB_SUCCESS;
  UNUSED(expr);
  return ret;
}

int ObRawExprPartFuncChecker::visit(ObVarRawExpr& expr)
{
  int ret = OB_SUCCESS;
  UNUSED(expr);
  return ret;
}

int ObRawExprPartFuncChecker::visit(ObQueryRefRawExpr& expr)
{
  int ret = OB_NOT_SUPPORTED;
  UNUSED(expr);
  return ret;
}

int ObRawExprPartFuncChecker::visit(ObColumnRefRawExpr& expr)
{
  int ret = OB_SUCCESS;
  UNUSED(expr);
  return ret;
}

int ObRawExprPartFuncChecker::visit(ObOpRawExpr& expr)
{
  int ret = OB_SUCCESS;
  switch (expr.get_expr_type()) {
    /* Bool operators */
    case T_OP_EQ:
    case T_OP_NSEQ:
    case T_OP_LE:
    case T_OP_LT:
    case T_OP_GE:
    case T_OP_GT:
    case T_OP_NE:
    /* bit operator */
    case T_OP_BIT_OR:
    case T_OP_BIT_AND:
    case T_OP_BIT_XOR:
    case T_OP_BIT_NEG:
    case T_OP_BIT_LEFT_SHIFT:
    case T_OP_BIT_RIGHT_SHIFT: {
      ret = OB_ERR_PARTITION_FUNCTION_IS_NOT_ALLOWED;
      LOG_WARN("invalid partition function", K(ret), "item_type", expr.get_expr_type());
      break;
    }
    case T_OP_DIV:  // /
    {
      if (is_oracle_mode() && gen_col_check_) {
        ret = OB_SUCCESS;
      } else {
        ret = OB_ERR_PARTITION_FUNCTION_IS_NOT_ALLOWED;
        LOG_WARN("invalid partition function", K(ret), "item_type", expr.get_expr_type());
      }
      break;
    }
    case T_OP_ADD:    // +
    case T_OP_MINUS:  // -
    case T_OP_MUL:    // *
    case T_OP_MOD:    // %
    {
      if (is_mysql_mode() || (is_oracle_mode() && gen_col_check_)) {
        ret = OB_SUCCESS;
      } else {
        ret = OB_ERR_PARTITION_FUNCTION_IS_NOT_ALLOWED;
        LOG_WARN("invalid partition function", K(ret), "item_type", expr.get_expr_type());
      }
      break;
    }
    default: {
      break;
    }
  }
  return ret;
}

int ObRawExprPartFuncChecker::visit(ObCaseOpRawExpr& expr)
{
  int ret = OB_ERR_PARTITION_FUNCTION_IS_NOT_ALLOWED;
  LOG_WARN("invalid partition function", K(ret), "item_type", expr.get_expr_type());
  return ret;
}

int ObRawExprPartFuncChecker::visit(ObAggFunRawExpr& expr)
{
  int ret = OB_ERR_PARTITION_FUNCTION_IS_NOT_ALLOWED;
  LOG_WARN("invalid partition function", K(ret), "item_type", expr.get_expr_type());
  return ret;
}

int ObRawExprPartFuncChecker::visit(ObSysFunRawExpr& expr)
{
  int ret = OB_SUCCESS;
  // white list, some of them are not implemented now
  switch (expr.get_expr_type()) {
    case T_FUN_SYS_DAY_OF_MONTH:
    case T_FUN_SYS_DAY_OF_WEEK:
    case T_FUN_SYS_DAY_OF_YEAR:
    case T_FUN_SYS_DATE_DIFF:  // DATEDIFF()
    case T_FUN_SYS_EXTRACT:
    // case T_FUN_SYS_EXTRACT: //EXTRACT()
    // case HOUR()
    case T_FUN_SYS_MICROSECOND:
    case T_FUN_SYS_MINUTE:
    // case MOD()
    case T_FUN_SYS_MONTH:  // MONTH()
    // case QUARTER()
    case T_FUN_SYS_SECOND:
    case T_FUN_SYS_TIME_TO_SEC:
    case T_FUN_SYS_TO_DAYS:    // TO_DAYS
    case T_FUN_SYS_FROM_DAYS:  // FROM_DAYS
    case T_FUN_SYS_TO_SECONDS:
    case T_FUN_SYS_TIME_TO_USEC:     // TIME_TO_USEC only exist in OB
    case T_FUN_SYS_UNIX_TIMESTAMP:   // UNIX_TIMESATMP()
    case T_FUN_SYS_WEEKDAY_OF_DATE:  // case WEEKDAY()
    case T_FUN_SYS_YEAR:
    case T_FUN_SYS_YEARWEEK_OF_DATE:  // case YEARKWEEK()
    case T_FUN_SYS_WEEK_OF_YEAR:      // case WEEKOFYEAR()
    case T_FUN_SYS_ADDR_TO_PART_ID:
    case T_FUN_SYS_TO_DATE:
    case T_FUN_SYS_TO_TIMESTAMP:
    case T_FUN_SYS_TO_TIMESTAMP_TZ:
    case T_FUN_SYS_TO_NUMBER:  // case TO_NUMBER()
    case T_FUN_SYS_TO_CHAR: {
      ret = OB_SUCCESS;
      break;
    }
    case T_FUN_SYS_CAST: {
      if (expr.has_flag(IS_INNER_ADDED_EXPR)) {
        ret = OB_SUCCESS;
      } else {
        ret = OB_ERR_PARTITION_FUNCTION_IS_NOT_ALLOWED;
        LOG_WARN("invalid partition function", K(ret), K(expr));
      }
      break;
    }
    case T_FUN_SYS_SUBSTR:
    case T_FUN_SYS_SUBSTRING_INDEX:
    case T_OP_CNN: {
      if (gen_col_check_) {
        ret = OB_SUCCESS;
      } else {
        ret = OB_ERR_PARTITION_FUNCTION_IS_NOT_ALLOWED;
        LOG_WARN("invalid partition function", K(ret), "item_type", expr.get_expr_type());
      }
      break;
    }
    case T_FUN_SYS_CHARSET:
    case T_FUN_SYS_SET_COLLATION: {
      if (accept_charset_function_) {
        ret = OB_SUCCESS;
      } else {
        ret = OB_ERR_PARTITION_FUNCTION_IS_NOT_ALLOWED;
        LOG_WARN("invalid partition function", K(ret), "item_type", expr.get_expr_type());
      }
      break;
    }
    case T_OP_ABS:        // ABS()
    case T_FUN_SYS_CEIL:  // CEILING()
    case T_FUN_SYS_CEILING:
    case T_FUN_SYS_FLOOR:  // FLOOR()
    {
      if (is_mysql_mode() || (is_oracle_mode() && gen_col_check_)) {
        ret = OB_SUCCESS;
      } else {
        ret = OB_ERR_PARTITION_FUNCTION_IS_NOT_ALLOWED;
        LOG_WARN("invalid partition function", K(ret), "item_type", expr.get_expr_type());
      }
      break;
    }
    case T_FUN_SYS_RPAD: {
      if (lib::is_oracle_mode()) {
        ret = OB_SUCCESS;
      } else {
        ret = OB_ERR_PARTITION_FUNCTION_IS_NOT_ALLOWED;
        LOG_WARN("invalid partition function", K(ret), "item_type", expr.get_expr_type());
      }
      break;
    }
    default: {
      if (is_oracle_mode() && gen_col_check_) {
        if (expr.is_udf_expr()) {
          ret = OB_ERR_PARTITION_FUNCTION_IS_NOT_ALLOWED;
          LOG_WARN("udf expr in partition function", K(ret), "item_type", expr.get_expr_type());
        } else {
          ret = OB_SUCCESS;
        }
      } else {
        ret = OB_ERR_PARTITION_FUNCTION_IS_NOT_ALLOWED;
        LOG_WARN("invalid partition function", K(ret), "item_type", expr.get_expr_type());
      }
    }
  }
  return ret;
}

int ObRawExprPartFuncChecker::visit(ObSetOpRawExpr& expr)
{
  int ret = OB_NOT_SUPPORTED;
  UNUSED(expr);
  return ret;
}

int ObRawExprPartFuncChecker::visit(ObAliasRefRawExpr& expr)
{
  int ret = OB_NOT_SUPPORTED;
  UNUSED(expr);
  return ret;
}

}  // namespace sql
}  // namespace oceanbase
