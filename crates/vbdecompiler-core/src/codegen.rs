// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

//! VB6 Code Generator
//!
//! Generates readable VB6 source code from IR (Intermediate Representation).
//!
//! This module handles:
//! - Function/Sub declarations
//! - Variable declarations
//! - Statement generation
//! - Expression generation with proper VB6 syntax
//! - Basic control flow generation
//! - Proper indentation

use crate::ir::*;

/// VB6 Code Generator
pub struct VB6CodeGenerator {
    indent_level: usize,
}

impl VB6CodeGenerator {
    pub fn new() -> Self {
        Self { indent_level: 0 }
    }

    /// Generate VB6 code for a complete function
    pub fn generate_function(&mut self, function: &Function) -> String {
        let mut code = String::new();

        // Generate function header
        code.push_str(&self.generate_function_header(function));
        code.push('\n');

        self.indent_level += 1;

        // Generate local variable declarations
        if !function.local_variables.is_empty() {
            code.push_str(&self.generate_local_variables(function));
            code.push('\n');
        }

        // Generate function body (statements from basic blocks)
        code.push_str(&self.generate_function_body(function));

        self.indent_level -= 1;

        // Generate function footer
        code.push_str(&self.generate_function_footer(function));

        code
    }

    /// Generate function header
    fn generate_function_header(&self, function: &Function) -> String {
        let func_type = if function.return_type.kind == TypeKind::Void {
            "Sub"
        } else {
            "Function"
        };

        let params = function
            .parameters
            .iter()
            .map(|p| format!("{} As {}", p.name, self.format_type_kind(p.var_type)))
            .collect::<Vec<_>>()
            .join(", ");

        if function.return_type.kind == TypeKind::Void {
            format!("{} {}({})", func_type, function.name, params)
        } else {
            format!(
                "{} {}({}) As {}",
                func_type,
                function.name,
                params,
                self.format_type(&function.return_type)
            )
        }
    }

    /// Generate function footer
    fn generate_function_footer(&self, function: &Function) -> String {
        let func_type = if function.return_type.kind == TypeKind::Void {
            "Sub"
        } else {
            "Function"
        };
        format!("End {}", func_type)
    }

    /// Generate local variable declarations
    fn generate_local_variables(&self, function: &Function) -> String {
        let mut code = String::new();

        for var in &function.local_variables {
            code.push_str(&self.indent());
            code.push_str(&format!(
                "Dim {} As {}\n",
                var.name,
                self.format_type_kind(var.var_type)
            ));
        }

        code
    }

    /// Generate function body from basic blocks
    fn generate_function_body(&mut self, function: &Function) -> String {
        let mut code = String::new();

        // Process blocks in order (simplified - assumes sequential order)
        for block in &function.basic_blocks {
            // Skip if block is entry and has no statements (common for structured code)
            if block.statements.is_empty() {
                continue;
            }

            // Add block label if it has multiple predecessors (merge point)
            if block.predecessors.len() > 1 {
                code.push_str(&format!("Block{}:\n", block.id));
            }

            // Generate statements
            for stmt in &block.statements {
                code.push_str(&self.generate_statement(stmt));
            }
        }

        code
    }

    /// Generate a statement
    pub fn generate_statement(&self, stmt: &Statement) -> String {
        let mut code = self.indent();

        match &stmt.data {
            StatementData::None => {
                code.push_str("' NOP\n");
            }
            StatementData::Assign { target, value } => {
                code.push_str(&format!(
                    "{} = {}\n",
                    target.name,
                    self.generate_expression(value)
                ));
            }
            StatementData::Store { address, value } => {
                code.push_str(&format!(
                    "[{}] = {}\n",
                    self.generate_expression(address),
                    self.generate_expression(value)
                ));
            }
            StatementData::Call {
                function,
                arguments,
            } => {
                if arguments.is_empty() {
                    code.push_str(&format!("{}\n", function));
                } else {
                    let args = arguments
                        .iter()
                        .map(|a| self.generate_expression(a))
                        .collect::<Vec<_>>()
                        .join(", ");
                    code.push_str(&format!("{} {}\n", function, args));
                }
            }
            StatementData::Return { value } => {
                if let Some(v) = value {
                    code.push_str(&format!(
                        "{} = {}\n",
                        "ReturnValue",
                        self.generate_expression(v)
                    ));
                    code.push_str(&self.indent());
                    code.push_str("Exit Function\n");
                } else {
                    code.push_str("Exit Sub\n");
                }
            }
            StatementData::Branch {
                condition,
                target_block,
            } => {
                code.push_str(&format!(
                    "If {} Then GoTo Block{}\n",
                    self.generate_expression(condition),
                    target_block
                ));
            }
            StatementData::Goto { target_block } => {
                code.push_str(&format!("GoTo Block{}\n", target_block));
            }
            StatementData::Label { label_id } => {
                code = format!("Label{}:\n", label_id);
            }
        }

        code
    }

    /// Generate an expression
    pub fn generate_expression(&self, expr: &Expression) -> String {
        match &expr.data {
            ExpressionData::None => String::new(),
            ExpressionData::Constant(val) => self.generate_constant(val),
            ExpressionData::Variable(var) => var.name.clone(),
            ExpressionData::Unary(operand) => {
                let op = match expr.kind {
                    ExpressionKind::Negate => "-",
                    ExpressionKind::Not => "Not ",
                    _ => "?",
                };
                format!("{}{}", op, self.generate_expression(operand))
            }
            ExpressionData::Binary { left, right } => {
                let op = self.get_binary_operator(expr.kind);
                format!(
                    "({} {} {})",
                    self.generate_expression(left),
                    op,
                    self.generate_expression(right)
                )
            }
            ExpressionData::Call {
                function,
                arguments,
            } => {
                if arguments.is_empty() {
                    format!("{}()", function)
                } else {
                    let args = arguments
                        .iter()
                        .map(|a| self.generate_expression(a))
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("{}({})", function, args)
                }
            }
            ExpressionData::MemberAccess { object, member } => {
                format!("{}.{}", self.generate_expression(object), member)
            }
            ExpressionData::ArrayIndex { array, indices } => {
                let idx = indices
                    .iter()
                    .map(|i| self.generate_expression(i))
                    .collect::<Vec<_>>()
                    .join(", ");
                format!("{}({})", self.generate_expression(array), idx)
            }
            ExpressionData::Cast { expr, target_type } => {
                format!(
                    "CType({}, {})",
                    self.generate_expression(expr),
                    self.format_type(target_type)
                )
            }
        }
    }

    /// Generate a constant value
    fn generate_constant(&self, value: &ConstantValue) -> String {
        match value {
            ConstantValue::Integer(v) => v.to_string(),
            ConstantValue::Float(v) => v.to_string(),
            ConstantValue::String(s) => format!("\"{}\"", s),
            ConstantValue::Boolean(b) => {
                if *b {
                    "True".to_string()
                } else {
                    "False".to_string()
                }
            }
        }
    }

    /// Get binary operator string
    fn get_binary_operator(&self, kind: ExpressionKind) -> &'static str {
        match kind {
            ExpressionKind::Add => "+",
            ExpressionKind::Subtract => "-",
            ExpressionKind::Multiply => "*",
            ExpressionKind::Divide => "/",
            ExpressionKind::IntDivide => "\\",
            ExpressionKind::Modulo => "Mod",
            ExpressionKind::Equal => "=",
            ExpressionKind::NotEqual => "<>",
            ExpressionKind::LessThan => "<",
            ExpressionKind::LessEqual => "<=",
            ExpressionKind::GreaterThan => ">",
            ExpressionKind::GreaterEqual => ">=",
            ExpressionKind::And => "And",
            ExpressionKind::Or => "Or",
            ExpressionKind::Xor => "Xor",
            ExpressionKind::Concatenate => "&",
            _ => "?",
        }
    }

    /// Format a type kind
    fn format_type_kind(&self, kind: TypeKind) -> &'static str {
        match kind {
            TypeKind::Void => "Void",
            TypeKind::Byte => "Byte",
            TypeKind::Boolean => "Boolean",
            TypeKind::Integer => "Integer",
            TypeKind::Long => "Long",
            TypeKind::Single => "Single",
            TypeKind::Double => "Double",
            TypeKind::Currency => "Currency",
            TypeKind::Date => "Date",
            TypeKind::String => "String",
            TypeKind::Object => "Object",
            TypeKind::Variant => "Variant",
            TypeKind::UserDefined => "UserDefined",
            TypeKind::Array => "Array",
            TypeKind::Unknown => "Variant",
        }
    }

    /// Format a type
    fn format_type(&self, ty: &Type) -> String {
        match ty.kind {
            TypeKind::Array => {
                if let Some(element_type) = &ty.element_type {
                    format!("{}()", self.format_type(element_type))
                } else {
                    "Array".to_string()
                }
            }
            TypeKind::UserDefined => {
                if let Some(name) = &ty.type_name {
                    name.clone()
                } else {
                    "UserDefined".to_string()
                }
            }
            _ => self.format_type_kind(ty.kind).to_string(),
        }
    }

    /// Get current indentation string
    fn indent(&self) -> String {
        "    ".repeat(self.indent_level)
    }
}

impl Default for VB6CodeGenerator {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_generate_function_header() {
        let gen = VB6CodeGenerator::new();

        // Test Sub (void return)
        let func1 = Function::new("TestSub".to_string(), Type::new(TypeKind::Void));
        assert!(gen
            .generate_function_header(&func1)
            .starts_with("Sub TestSub("));

        // Test Function (non-void return)
        let func2 = Function::new("TestFunc".to_string(), Type::new(TypeKind::Integer));
        assert!(gen
            .generate_function_header(&func2)
            .starts_with("Function TestFunc("));
    }

    #[test]
    fn test_generate_expression() {
        let gen = VB6CodeGenerator::new();

        // Test constant
        let const_expr = Expression::int_const(42);
        assert_eq!(gen.generate_expression(&const_expr), "42");

        // Test string constant
        let str_expr = Expression::string_const("Hello".to_string());
        assert_eq!(gen.generate_expression(&str_expr), "\"Hello\"");

        // Test variable
        let var = Variable::new(0, "x".to_string(), TypeKind::Integer);
        let var_expr = Expression::variable(var);
        assert_eq!(gen.generate_expression(&var_expr), "x");
    }

    #[test]
    fn test_generate_statement() {
        let gen = VB6CodeGenerator::new();

        // Test assignment
        let var = Variable::new(0, "x".to_string(), TypeKind::Integer);
        let value = Expression::int_const(10);
        let stmt = Statement::assign(var, value);
        let code = gen.generate_statement(&stmt);
        assert!(code.contains("x = 10"));

        // Test return
        let ret_stmt = Statement::return_stmt(Some(Expression::int_const(5)));
        let ret_code = gen.generate_statement(&ret_stmt);
        assert!(ret_code.contains("ReturnValue = 5"));
        assert!(ret_code.contains("Exit Function"));
    }

    #[test]
    fn test_binary_operators() {
        let gen = VB6CodeGenerator::new();

        let left = Expression::int_const(1);
        let right = Expression::int_const(2);

        let add_expr = Expression::add(left.clone(), right.clone(), Type::new(TypeKind::Integer));
        assert!(gen.generate_expression(&add_expr).contains("+"));

        let eq_expr = Expression::equal(left, right);
        assert!(gen.generate_expression(&eq_expr).contains("="));
    }
}
