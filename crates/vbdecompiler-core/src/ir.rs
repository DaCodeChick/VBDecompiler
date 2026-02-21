// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

//! Intermediate Representation (IR) module
//!
//! Defines the IR used during decompilation:
//! - Types (VB data types)
//! - Expressions (operations, variables, constants)
//! - Statements (assignments, calls, control flow)
//! - Basic blocks and functions

use std::fmt;

/// VB Type Kind - Represents Visual Basic data types
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum TypeKind {
    Void,        // No type (for procedures without return value)
    Byte,        // 8-bit unsigned integer
    Boolean,     // True/False
    Integer,     // 16-bit signed integer
    Long,        // 32-bit signed integer
    Single,      // 32-bit floating point
    Double,      // 64-bit floating point
    Currency,    // Fixed-point currency type
    Date,        // Date/time value
    String,      // Variable-length string
    Object,      // Object reference
    Variant,     // Variant type (can hold any type)
    UserDefined, // User-defined type (UDT)
    Array,       // Array type
    Unknown,     // Unknown/unresolved type
}

impl TypeKind {
    /// Get the size in bytes for this type
    pub fn size(&self) -> u32 {
        match self {
            Self::Void => 0,
            Self::Byte | Self::Boolean => 1,
            Self::Integer => 2,
            Self::Long | Self::Single => 4,
            Self::Double | Self::Currency | Self::Date => 8,
            Self::String | Self::Object | Self::Variant => 4, // Pointer size
            Self::Array | Self::UserDefined | Self::Unknown => 4,
        }
    }

    /// Check if this is a numeric type
    pub fn is_numeric(&self) -> bool {
        matches!(
            self,
            Self::Byte | Self::Integer | Self::Long | Self::Single | Self::Double | Self::Currency
        )
    }

    /// Check if this is an integer type
    pub fn is_integer(&self) -> bool {
        matches!(self, Self::Byte | Self::Integer | Self::Long)
    }

    /// Check if this is a floating point type
    pub fn is_floating_point(&self) -> bool {
        matches!(self, Self::Single | Self::Double)
    }

    /// Check if this is a reference type
    pub fn is_reference(&self) -> bool {
        matches!(self, Self::String | Self::Object | Self::Array)
    }
}

impl fmt::Display for TypeKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let name = match self {
            Self::Void => "Void",
            Self::Byte => "Byte",
            Self::Boolean => "Boolean",
            Self::Integer => "Integer",
            Self::Long => "Long",
            Self::Single => "Single",
            Self::Double => "Double",
            Self::Currency => "Currency",
            Self::Date => "Date",
            Self::String => "String",
            Self::Object => "Object",
            Self::Variant => "Variant",
            Self::UserDefined => "UserDefined",
            Self::Array => "Array",
            Self::Unknown => "Unknown",
        };
        write!(f, "{}", name)
    }
}

/// IR Type - Represents a type in the intermediate representation
#[derive(Debug, Clone, PartialEq)]
pub struct Type {
    pub kind: TypeKind,
    pub element_type: Option<Box<Type>>, // For array types
    pub array_dimensions: usize,
    pub type_name: Option<String>, // For user-defined types
}

impl Type {
    /// Create a basic type
    pub fn new(kind: TypeKind) -> Self {
        Self {
            kind,
            element_type: None,
            array_dimensions: 0,
            type_name: None,
        }
    }

    /// Create an array type
    pub fn array(element_type: Type, dimensions: usize) -> Self {
        Self {
            kind: TypeKind::Array,
            element_type: Some(Box::new(element_type)),
            array_dimensions: dimensions,
            type_name: None,
        }
    }

    /// Create a user-defined type
    pub fn user_defined(name: String) -> Self {
        Self {
            kind: TypeKind::UserDefined,
            element_type: None,
            array_dimensions: 0,
            type_name: Some(name),
        }
    }
}

impl fmt::Display for Type {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match &self.kind {
            TypeKind::Array => {
                write!(
                    f,
                    "{}({})",
                    self.element_type.as_ref().unwrap(),
                    self.array_dimensions
                )
            }
            TypeKind::UserDefined => write!(f, "{}", self.type_name.as_ref().unwrap()),
            _ => write!(f, "{}", self.kind),
        }
    }
}

/// Expression Kind - Types of IR expressions
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ExpressionKind {
    // Literals
    Constant,
    // Variables
    Variable,
    Temporary,
    // Unary operations
    Negate,
    Not,
    // Binary operations - Arithmetic
    Add,
    Subtract,
    Multiply,
    Divide,
    IntDivide,
    Modulo,
    // Binary operations - Comparison
    Equal,
    NotEqual,
    LessThan,
    LessEqual,
    GreaterThan,
    GreaterEqual,
    // Binary operations - Logical
    And,
    Or,
    Xor,
    // Binary operations - String
    Concatenate,
    // Memory operations
    Load,
    MemberAccess,
    ArrayIndex,
    // Function call
    Call,
    // Type conversion
    Cast,
}

/// Constant value
#[derive(Debug, Clone)]
pub enum ConstantValue {
    Integer(i64),
    Float(f64),
    String(String),
    Boolean(bool),
}

impl fmt::Display for ConstantValue {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Integer(v) => write!(f, "{}", v),
            Self::Float(v) => write!(f, "{}", v),
            Self::String(s) => write!(f, "\"{}\"", s),
            Self::Boolean(b) => write!(f, "{}", if *b { "True" } else { "False" }),
        }
    }
}

/// Variable reference
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Variable {
    pub id: u32,
    pub name: String,
    pub var_type: TypeKind,
}

impl Variable {
    pub fn new(id: u32, name: String, var_type: TypeKind) -> Self {
        Self { id, name, var_type }
    }
}

impl fmt::Display for Variable {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.name)
    }
}

/// IR Expression
#[derive(Debug, Clone)]
pub struct Expression {
    pub kind: ExpressionKind,
    pub expr_type: Type,
    pub data: ExpressionData,
}

/// Expression data payload
#[derive(Debug, Clone)]
pub enum ExpressionData {
    None,
    Constant(ConstantValue),
    Variable(Variable),
    Unary(Box<Expression>),
    Binary {
        left: Box<Expression>,
        right: Box<Expression>,
    },
    Call {
        function: String,
        arguments: Vec<Expression>,
    },
    MemberAccess {
        object: Box<Expression>,
        member: String,
    },
    ArrayIndex {
        array: Box<Expression>,
        indices: Vec<Expression>,
    },
    Cast {
        expr: Box<Expression>,
        target_type: Type,
    },
}

impl Expression {
    /// Create a constant expression
    pub fn constant(value: ConstantValue, expr_type: Type) -> Self {
        Self {
            kind: ExpressionKind::Constant,
            expr_type,
            data: ExpressionData::Constant(value),
        }
    }

    /// Create an integer constant
    pub fn int_const(value: i64) -> Self {
        Self::constant(ConstantValue::Integer(value), Type::new(TypeKind::Long))
    }

    /// Create a string constant
    pub fn string_const(value: String) -> Self {
        Self::constant(ConstantValue::String(value), Type::new(TypeKind::String))
    }

    /// Create a boolean constant
    pub fn bool_const(value: bool) -> Self {
        Self::constant(ConstantValue::Boolean(value), Type::new(TypeKind::Boolean))
    }

    /// Create a variable reference
    pub fn variable(var: Variable) -> Self {
        let var_type = Type::new(var.var_type);
        Self {
            kind: ExpressionKind::Variable,
            expr_type: var_type,
            data: ExpressionData::Variable(var),
        }
    }

    /// Create a binary operation
    pub fn binary(
        kind: ExpressionKind,
        left: Expression,
        right: Expression,
        result_type: Type,
    ) -> Self {
        Self {
            kind,
            expr_type: result_type,
            data: ExpressionData::Binary {
                left: Box::new(left),
                right: Box::new(right),
            },
        }
    }

    /// Create an add expression
    pub fn add(left: Expression, right: Expression, result_type: Type) -> Self {
        Self::binary(ExpressionKind::Add, left, right, result_type)
    }

    /// Create a comparison expression
    pub fn equal(left: Expression, right: Expression) -> Self {
        Self::binary(
            ExpressionKind::Equal,
            left,
            right,
            Type::new(TypeKind::Boolean),
        )
    }

    /// Create a function call expression
    pub fn call(function: String, arguments: Vec<Expression>, return_type: Type) -> Self {
        Self {
            kind: ExpressionKind::Call,
            expr_type: return_type,
            data: ExpressionData::Call {
                function,
                arguments,
            },
        }
    }

    /// Convert expression to VB6 source code string (simplified)
    pub fn to_vb_string(&self) -> String {
        match &self.data {
            ExpressionData::None => String::from(""),
            ExpressionData::Constant(val) => format!("{}", val),
            ExpressionData::Variable(var) => format!("{}", var),
            ExpressionData::Unary(expr) => {
                let op = match self.kind {
                    ExpressionKind::Negate => "-",
                    ExpressionKind::Not => "Not ",
                    _ => "",
                };
                format!("{}{}", op, expr.to_vb_string())
            }
            ExpressionData::Binary { left, right } => {
                let op = match self.kind {
                    ExpressionKind::Add => " + ",
                    ExpressionKind::Subtract => " - ",
                    ExpressionKind::Multiply => " * ",
                    ExpressionKind::Divide => " / ",
                    ExpressionKind::IntDivide => " \\ ",
                    ExpressionKind::Modulo => " Mod ",
                    ExpressionKind::Equal => " = ",
                    ExpressionKind::NotEqual => " <> ",
                    ExpressionKind::LessThan => " < ",
                    ExpressionKind::LessEqual => " <= ",
                    ExpressionKind::GreaterThan => " > ",
                    ExpressionKind::GreaterEqual => " >= ",
                    ExpressionKind::And => " And ",
                    ExpressionKind::Or => " Or ",
                    ExpressionKind::Xor => " Xor ",
                    ExpressionKind::Concatenate => " & ",
                    _ => " ? ",
                };
                format!("({}{}{})", left.to_vb_string(), op, right.to_vb_string())
            }
            ExpressionData::Call {
                function,
                arguments,
            } => {
                let args = arguments
                    .iter()
                    .map(|a| a.to_vb_string())
                    .collect::<Vec<_>>()
                    .join(", ");
                format!("{}({})", function, args)
            }
            ExpressionData::MemberAccess { object, member } => {
                format!("{}.{}", object.to_vb_string(), member)
            }
            ExpressionData::ArrayIndex { array, indices } => {
                let idx = indices
                    .iter()
                    .map(|i| i.to_vb_string())
                    .collect::<Vec<_>>()
                    .join(", ");
                format!("{}({})", array.to_vb_string(), idx)
            }
            ExpressionData::Cast { expr, target_type } => {
                format!("CType({}, {})", expr.to_vb_string(), target_type)
            }
        }
    }
}

/// Statement Kind - Types of IR statements
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum StatementKind {
    Assign, // variable = expression
    Store,  // [address] = expression
    Call,   // Call subroutine (no return value)
    Return, // Return [expression]
    Branch, // Conditional branch
    Goto,   // Unconditional jump
    Label,  // Label marker
    Nop,    // No operation
}

/// IR Statement
#[derive(Debug, Clone)]
pub struct Statement {
    pub kind: StatementKind,
    pub data: StatementData,
}

/// Statement data payload
#[derive(Debug, Clone)]
pub enum StatementData {
    None,
    Assign {
        target: Variable,
        value: Expression,
    },
    Store {
        address: Expression,
        value: Expression,
    },
    Call {
        function: String,
        arguments: Vec<Expression>,
    },
    Return {
        value: Option<Expression>,
    },
    Branch {
        condition: Expression,
        target_block: u32,
    },
    Goto {
        target_block: u32,
    },
    Label {
        label_id: u32,
    },
}

impl Statement {
    /// Create an assignment statement
    pub fn assign(target: Variable, value: Expression) -> Self {
        Self {
            kind: StatementKind::Assign,
            data: StatementData::Assign { target, value },
        }
    }

    /// Create a call statement
    pub fn call(function: String, arguments: Vec<Expression>) -> Self {
        Self {
            kind: StatementKind::Call,
            data: StatementData::Call {
                function,
                arguments,
            },
        }
    }

    /// Create a return statement
    pub fn return_stmt(value: Option<Expression>) -> Self {
        Self {
            kind: StatementKind::Return,
            data: StatementData::Return { value },
        }
    }

    /// Create a branch statement
    pub fn branch(condition: Expression, target_block: u32) -> Self {
        Self {
            kind: StatementKind::Branch,
            data: StatementData::Branch {
                condition,
                target_block,
            },
        }
    }

    /// Create a goto statement
    pub fn goto(target_block: u32) -> Self {
        Self {
            kind: StatementKind::Goto,
            data: StatementData::Goto { target_block },
        }
    }

    /// Create a label statement
    pub fn label(label_id: u32) -> Self {
        Self {
            kind: StatementKind::Label,
            data: StatementData::Label { label_id },
        }
    }

    /// Create a NOP statement
    pub fn nop() -> Self {
        Self {
            kind: StatementKind::Nop,
            data: StatementData::None,
        }
    }

    /// Convert statement to VB6 source code string (simplified)
    pub fn to_vb_string(&self) -> String {
        match &self.data {
            StatementData::None => String::from("' NOP"),
            StatementData::Assign { target, value } => {
                format!("{} = {}", target, value.to_vb_string())
            }
            StatementData::Store { address, value } => {
                format!("[{}] = {}", address.to_vb_string(), value.to_vb_string())
            }
            StatementData::Call {
                function,
                arguments,
            } => {
                let args = arguments
                    .iter()
                    .map(|a| a.to_vb_string())
                    .collect::<Vec<_>>()
                    .join(", ");
                if args.is_empty() {
                    format!("{}", function)
                } else {
                    format!("{} {}", function, args)
                }
            }
            StatementData::Return { value } => {
                if let Some(v) = value {
                    format!("Return {}", v.to_vb_string())
                } else {
                    String::from("Exit Sub")
                }
            }
            StatementData::Branch {
                condition,
                target_block,
            } => {
                format!(
                    "If {} Then Goto Block{}",
                    condition.to_vb_string(),
                    target_block
                )
            }
            StatementData::Goto { target_block } => {
                format!("Goto Block{}", target_block)
            }
            StatementData::Label { label_id } => {
                format!("Label{}:", label_id)
            }
        }
    }
}

/// Basic Block - A sequence of statements with single entry and exit
#[derive(Debug, Clone)]
pub struct BasicBlock {
    pub id: u32,
    pub statements: Vec<Statement>,
    pub successors: Vec<u32>,   // Block IDs of successor blocks
    pub predecessors: Vec<u32>, // Block IDs of predecessor blocks
}

impl BasicBlock {
    pub fn new(id: u32) -> Self {
        Self {
            id,
            statements: Vec::new(),
            successors: Vec::new(),
            predecessors: Vec::new(),
        }
    }

    pub fn add_statement(&mut self, stmt: Statement) {
        self.statements.push(stmt);
    }

    pub fn add_successor(&mut self, block_id: u32) {
        if !self.successors.contains(&block_id) {
            self.successors.push(block_id);
        }
    }

    pub fn add_predecessor(&mut self, block_id: u32) {
        if !self.predecessors.contains(&block_id) {
            self.predecessors.push(block_id);
        }
    }
}

/// IR Function - Represents a complete function/subroutine
#[derive(Debug, Clone)]
pub struct Function {
    pub name: String,
    pub return_type: Type,
    pub parameters: Vec<Variable>,
    pub local_variables: Vec<Variable>,
    pub basic_blocks: Vec<BasicBlock>,
    pub entry_block_id: u32,
}

impl Function {
    pub fn new(name: String, return_type: Type) -> Self {
        Self {
            name,
            return_type,
            parameters: Vec::new(),
            local_variables: Vec::new(),
            basic_blocks: Vec::new(),
            entry_block_id: 0,
        }
    }

    pub fn add_parameter(&mut self, param: Variable) {
        self.parameters.push(param);
    }

    pub fn add_local_variable(&mut self, var: Variable) {
        self.local_variables.push(var);
    }

    pub fn add_basic_block(&mut self, block: BasicBlock) {
        self.basic_blocks.push(block);
    }

    pub fn get_block(&self, id: u32) -> Option<&BasicBlock> {
        self.basic_blocks.iter().find(|b| b.id == id)
    }

    pub fn get_block_mut(&mut self, id: u32) -> Option<&mut BasicBlock> {
        self.basic_blocks.iter_mut().find(|b| b.id == id)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_type_creation() {
        let int_type = Type::new(TypeKind::Integer);
        assert_eq!(int_type.kind, TypeKind::Integer);
        assert!(int_type.kind.is_integer());
        assert!(int_type.kind.is_numeric());
    }

    #[test]
    fn test_expression_creation() {
        let expr = Expression::int_const(42);
        assert_eq!(expr.kind, ExpressionKind::Constant);
        assert_eq!(expr.to_vb_string(), "42");
    }

    #[test]
    fn test_binary_expression() {
        let left = Expression::int_const(1);
        let right = Expression::int_const(2);
        let expr = Expression::add(left, right, Type::new(TypeKind::Integer));
        assert_eq!(expr.to_vb_string(), "(1 + 2)");
    }

    #[test]
    fn test_statement_creation() {
        let var = Variable::new(0, "x".to_string(), TypeKind::Integer);
        let value = Expression::int_const(10);
        let stmt = Statement::assign(var, value);
        assert_eq!(stmt.kind, StatementKind::Assign);
        assert_eq!(stmt.to_vb_string(), "x = 10");
    }
}
