// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

//! Intermediate representation (IR) module

/// IR function
#[derive(Debug, Clone)]
pub struct IRFunction {
    pub name: String,
    pub blocks: Vec<IRBasicBlock>,
}

/// IR basic block
#[derive(Debug, Clone)]
pub struct IRBasicBlock {
    pub id: u32,
    pub statements: Vec<IRStatement>,
}

/// IR statement
#[derive(Debug, Clone)]
pub enum IRStatement {
    Assignment {
        target: String,
        value: IRExpression,
    },
    Return {
        value: Option<IRExpression>,
    },
    Branch {
        condition: IRExpression,
        true_target: u32,
        false_target: u32,
    },
}

/// IR expression
#[derive(Debug, Clone)]
pub enum IRExpression {
    Variable(String),
    Constant(i32),
    Binary {
        op: BinaryOp,
        left: Box<IRExpression>,
        right: Box<IRExpression>,
    },
}

/// Binary operation
#[derive(Debug, Clone, Copy)]
pub enum BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Gt,
    Lt,
    Eq,
}
