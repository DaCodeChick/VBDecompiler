// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

//! P-Code to IR Lifter
//!
//! Converts Visual Basic P-Code instructions to IR (Intermediate Representation).
//! P-Code is a stack-based bytecode format, so this lifter maintains a virtual
//! evaluation stack and converts stack operations to SSA-style IR with variables.
//!
//! Architecture:
//! - P-Code pushes/pops values on a virtual stack
//! - Lifter converts stack operations to temporary variables (t0, t1, t2, ...)
//! - Creates BasicBlocks with CFG edges for branches
//! - Maps P-Code types to VB types in the IR type system

use crate::error::{Error, Result};
use crate::ir::*;
use crate::pcode::{Instruction, OpcodeCategory, OperandValue, PCodeType};
use std::collections::HashMap;

/// P-Code to IR Lifter
pub struct PCodeLifter {
    last_error: Option<String>,
}

impl PCodeLifter {
    pub fn new() -> Self {
        Self { last_error: None }
    }

    /// Lift a sequence of P-Code instructions to an IR function
    pub fn lift(
        &mut self,
        instructions: &[Instruction],
        function_name: String,
        start_address: u32,
    ) -> Result<Function> {
        if instructions.is_empty() {
            return Err(Error::Decompilation("No instructions to lift".to_string()));
        }

        // Create lifting context
        let mut ctx = LiftContext::new(function_name, start_address);

        // First pass: identify basic block boundaries (branch targets)
        for instr in instructions {
            if instr.is_branch {
                if let Some(offset) = instr.branch_offset {
                    if offset != 0 {
                        let instr_len = instr.bytes.len() as u32;
                        let target_addr = instr
                            .address
                            .wrapping_add(instr_len)
                            .wrapping_add(offset as u32);
                        ctx.get_or_create_block_for_address(target_addr);
                    }
                }
            }
        }

        // Second pass: lift instructions
        for instr in instructions {
            // Check if this address starts a new block
            if let Some(&block_id) = ctx.address_to_block.get(&instr.address) {
                if block_id != ctx.current_block_id {
                    // Connect current block to new block
                    if let Some(current_block) = ctx.function.get_block_mut(ctx.current_block_id) {
                        if !current_block.statements.is_empty() {
                            current_block.add_successor(block_id);
                        }
                    }
                    ctx.current_block_id = block_id;
                }
            }

            // Lift the instruction
            if let Err(e) = self.lift_instruction(instr, &mut ctx) {
                self.last_error = Some(format!("Failed to lift {}: {}", instr.mnemonic, e));
                return Err(e);
            }

            // Stop at return
            if instr.is_return {
                break;
            }
        }

        Ok(ctx.function)
    }

    /// Get last error message
    pub fn last_error(&self) -> Option<&str> {
        self.last_error.as_deref()
    }

    /// Lift a single instruction
    fn lift_instruction(&mut self, instr: &Instruction, ctx: &mut LiftContext) -> Result<()> {
        // Route to specialized lifters based on category
        match instr.category {
            OpcodeCategory::Arithmetic => self.lift_arithmetic(instr, ctx),
            OpcodeCategory::Comparison => self.lift_comparison(instr, ctx),
            OpcodeCategory::Logical => self.lift_logical(instr, ctx),
            OpcodeCategory::Stack | OpcodeCategory::Variable => self.lift_stack(instr, ctx),
            OpcodeCategory::Memory | OpcodeCategory::Array => self.lift_memory(instr, ctx),
            OpcodeCategory::ControlFlow => {
                if instr.is_branch {
                    self.lift_branch(instr, ctx)
                } else if instr.is_return
                    || instr.mnemonic.contains("Exit")
                    || instr.mnemonic.contains("Return")
                {
                    self.lift_return(instr, ctx)
                } else {
                    Ok(()) // Ignore other control flow for now
                }
            }
            OpcodeCategory::Call => self.lift_call(instr, ctx),
            _ => Ok(()), // Ignore unknown categories
        }
    }

    /// Lift arithmetic operations
    fn lift_arithmetic(&mut self, instr: &Instruction, ctx: &mut LiftContext) -> Result<()> {
        // Map P-Code arithmetic to IR binary operations
        let op = if instr.mnemonic.contains("Add") {
            ExpressionKind::Add
        } else if instr.mnemonic.contains("Sub") {
            ExpressionKind::Subtract
        } else if instr.mnemonic.contains("Mul") {
            ExpressionKind::Multiply
        } else if instr.mnemonic.contains("Div") {
            ExpressionKind::Divide
        } else if instr.mnemonic.contains("Idiv") {
            ExpressionKind::IntDivide
        } else if instr.mnemonic.contains("Mod") {
            ExpressionKind::Modulo
        } else if instr.mnemonic.contains("Concat") {
            ExpressionKind::Concatenate
        } else {
            return Ok(()); // Unknown arithmetic, skip
        };

        // Pop operands (right then left, since it's a stack)
        let right = ctx.pop_stack()?;
        let left = ctx.pop_stack()?;

        // Create binary expression
        let result = Expression::binary(op, left, right, Type::new(TypeKind::Variant));

        // Push result
        ctx.push_stack(result);

        Ok(())
    }

    /// Lift comparison operations
    fn lift_comparison(&mut self, instr: &Instruction, ctx: &mut LiftContext) -> Result<()> {
        // Map P-Code comparison to IR comparison operations
        let op = if instr.mnemonic.contains("Eq") {
            ExpressionKind::Equal
        } else if instr.mnemonic.contains("Ne") {
            ExpressionKind::NotEqual
        } else if instr.mnemonic.contains("Lt") {
            ExpressionKind::LessThan
        } else if instr.mnemonic.contains("Le") {
            ExpressionKind::LessEqual
        } else if instr.mnemonic.contains("Gt") {
            ExpressionKind::GreaterThan
        } else if instr.mnemonic.contains("Ge") {
            ExpressionKind::GreaterEqual
        } else {
            return Ok(()); // Unknown comparison, skip
        };

        // Pop operands
        let right = ctx.pop_stack()?;
        let left = ctx.pop_stack()?;

        // Create comparison expression
        let result = Expression::binary(op, left, right, Type::new(TypeKind::Boolean));

        // Push result
        ctx.push_stack(result);

        Ok(())
    }

    /// Lift logical operations
    fn lift_logical(&mut self, instr: &Instruction, ctx: &mut LiftContext) -> Result<()> {
        // Handle unary NOT
        if instr.mnemonic.contains("Not") {
            let operand = ctx.pop_stack()?;
            let result = Expression {
                kind: ExpressionKind::Not,
                expr_type: Type::new(TypeKind::Boolean),
                data: ExpressionData::Unary(Box::new(operand)),
            };
            ctx.push_stack(result);
            return Ok(());
        }

        // Map P-Code logical to IR logical operations
        let op = if instr.mnemonic.contains("And") {
            ExpressionKind::And
        } else if instr.mnemonic.contains("Or") {
            ExpressionKind::Or
        } else if instr.mnemonic.contains("Xor") {
            ExpressionKind::Xor
        } else {
            return Ok(()); // Unknown logical, skip
        };

        // Binary logical operations
        let right = ctx.pop_stack()?;
        let left = ctx.pop_stack()?;

        let result = Expression::binary(op, left, right, Type::new(TypeKind::Boolean));
        ctx.push_stack(result);

        Ok(())
    }

    /// Lift stack operations (literals and variable loads/stores)
    fn lift_stack(&mut self, instr: &Instruction, ctx: &mut LiftContext) -> Result<()> {
        // Handle literal pushes
        if instr.mnemonic.contains("Lit") {
            if instr.operands.is_empty() {
                return Err(Error::Decompilation("Literal with no operands".to_string()));
            }

            let operand = &instr.operands[0];
            let expr = match &operand.value {
                OperandValue::Byte(v) => Expression::int_const(*v as i64),
                OperandValue::Int16(v) => Expression::int_const(*v as i64),
                OperandValue::Int32(v) => Expression::int_const(*v as i64),
                OperandValue::Float(v) => Expression::constant(
                    ConstantValue::Float(*v as f64),
                    Type::new(TypeKind::Single),
                ),
                OperandValue::String(s) => Expression::string_const(s.clone()),
                OperandValue::None => {
                    return Err(Error::Decompilation("Literal with None value".to_string()));
                }
            };

            ctx.push_stack(expr);
            return Ok(());
        }

        // Handle local variable loads
        if instr.mnemonic.contains("LdLoc") || instr.mnemonic.contains("LoadLocal") {
            if instr.operands.is_empty() {
                return Err(Error::Decompilation(
                    "LoadLocal with no operands".to_string(),
                ));
            }

            let local_index = match &instr.operands[0].value {
                OperandValue::Int16(v) => *v as u32,
                OperandValue::Int32(v) => *v as u32,
                OperandValue::Byte(v) => *v as u32,
                _ => {
                    return Err(Error::Decompilation(
                        "LoadLocal with invalid index type".to_string(),
                    ));
                }
            };
            let var_name = format!("local{}", local_index);
            let var_type = pcode_type_to_ir_type(instr.operands[0].data_type);

            let var = Variable::new(local_index, var_name, var_type);
            let expr = Expression::variable(var);
            ctx.push_stack(expr);
            return Ok(());
        }

        // Handle local variable stores
        if instr.mnemonic.contains("StLoc") || instr.mnemonic.contains("StoreLocal") {
            if instr.operands.is_empty() {
                return Err(Error::Decompilation(
                    "StoreLocal with no operands".to_string(),
                ));
            }

            let value = ctx.pop_stack()?;

            let local_index = match &instr.operands[0].value {
                OperandValue::Int16(v) => *v as u32,
                OperandValue::Int32(v) => *v as u32,
                OperandValue::Byte(v) => *v as u32,
                _ => {
                    return Err(Error::Decompilation(
                        "StoreLocal with invalid index type".to_string(),
                    ));
                }
            };
            let var_name = format!("local{}", local_index);
            let var_type = pcode_type_to_ir_type(instr.operands[0].data_type);

            let var = Variable::new(local_index, var_name, var_type);
            let stmt = Statement::assign(var, value);

            if let Some(block) = ctx.function.get_block_mut(ctx.current_block_id) {
                block.add_statement(stmt);
            }
            return Ok(());
        }

        Ok(())
    }

    /// Lift memory operations
    fn lift_memory(&mut self, _instr: &Instruction, _ctx: &mut LiftContext) -> Result<()> {
        // Memory operations - to be implemented when needed
        Ok(())
    }

    /// Lift branch operations
    fn lift_branch(&mut self, instr: &Instruction, ctx: &mut LiftContext) -> Result<()> {
        // Calculate branch target address
        let branch_offset = instr
            .branch_offset
            .ok_or_else(|| Error::Decompilation("Branch instruction with no offset".to_string()))?;

        let instr_len = instr.bytes.len() as u32;
        let target_addr = instr
            .address
            .wrapping_add(instr_len)
            .wrapping_add(branch_offset as u32);

        if instr.is_conditional_branch {
            // Pop condition from stack
            let condition = ctx.pop_stack()?;

            // Get or create target block
            let target_block_id = ctx.get_or_create_block_for_address(target_addr);

            // Create branch statement
            let stmt = Statement::branch(condition, target_block_id);

            // Add to current block
            if let Some(block) = ctx.function.get_block_mut(ctx.current_block_id) {
                block.add_statement(stmt);
                block.add_successor(target_block_id);
            }

            // Create fall-through block
            let fall_through_id = ctx.create_new_block();
            if let Some(block) = ctx.function.get_block_mut(ctx.current_block_id) {
                block.add_successor(fall_through_id);
            }
            ctx.current_block_id = fall_through_id;
        } else {
            // Unconditional branch (goto)
            let target_block_id = ctx.get_or_create_block_for_address(target_addr);

            let stmt = Statement::goto(target_block_id);

            // Add to current block
            if let Some(block) = ctx.function.get_block_mut(ctx.current_block_id) {
                block.add_statement(stmt);
                block.add_successor(target_block_id);
            }

            // Create new block for any following code
            ctx.current_block_id = ctx.create_new_block();
        }

        Ok(())
    }

    /// Lift call operations
    fn lift_call(&mut self, instr: &Instruction, ctx: &mut LiftContext) -> Result<()> {
        // Extract function name/address
        let func_name = if !instr.operands.is_empty() {
            let operand = &instr.operands[0];
            match &operand.value {
                OperandValue::Int32(v) => format!("func_{}", v),
                OperandValue::String(s) => s.clone(),
                OperandValue::Int16(v) => format!("func_{}", v),
                _ => "func_unknown".to_string(),
            }
        } else {
            "func_unknown".to_string()
        };

        // For now, create a simple call with no arguments
        // TODO: Pop arguments from stack based on calling convention
        let args = Vec::new();

        // If this is a function call (not sub), create call expression and push result
        if instr.mnemonic.contains("CallFunc") || instr.mnemonic.contains("CallI4") {
            let call_expr = Expression::call(func_name, args, Type::new(TypeKind::Variant));
            ctx.push_stack(call_expr);
        } else {
            // It's a subroutine call, create a call statement
            let stmt = Statement::call(func_name, args);
            if let Some(block) = ctx.function.get_block_mut(ctx.current_block_id) {
                block.add_statement(stmt);
            }
        }

        Ok(())
    }

    /// Lift return operations
    fn lift_return(&mut self, instr: &Instruction, ctx: &mut LiftContext) -> Result<()> {
        // Check if this is a function return (with value) or sub return (no value)
        let stmt = if instr.mnemonic.contains("ExitProc") {
            // Sub return - no value
            Statement::return_stmt(None)
        } else {
            // Function return - pop return value
            let ret_value = ctx.pop_stack().ok();
            Statement::return_stmt(ret_value)
        };

        if let Some(block) = ctx.function.get_block_mut(ctx.current_block_id) {
            block.add_statement(stmt);
        }

        Ok(())
    }
}

impl Default for PCodeLifter {
    fn default() -> Self {
        Self::new()
    }
}

/// Context for lifting a single function
struct LiftContext {
    function: Function,
    current_block_id: u32,
    eval_stack: Vec<Expression>,
    next_block_id: u32,
    address_to_block: HashMap<u32, u32>,
}

impl LiftContext {
    fn new(function_name: String, _start_address: u32) -> Self {
        let mut function = Function::new(function_name, Type::new(TypeKind::Variant));

        // Create entry block
        let entry_block = BasicBlock::new(0);
        function.add_basic_block(entry_block);
        function.entry_block_id = 0;

        Self {
            function,
            current_block_id: 0,
            eval_stack: Vec::new(),
            next_block_id: 1,
            address_to_block: HashMap::new(),
        }
    }

    fn pop_stack(&mut self) -> Result<Expression> {
        self.eval_stack
            .pop()
            .ok_or_else(|| Error::Decompilation("Stack underflow".to_string()))
    }

    fn push_stack(&mut self, expr: Expression) {
        self.eval_stack.push(expr);
    }

    fn create_new_block(&mut self) -> u32 {
        let block_id = self.next_block_id;
        self.next_block_id += 1;

        let block = BasicBlock::new(block_id);
        self.function.add_basic_block(block);

        block_id
    }

    fn get_or_create_block_for_address(&mut self, address: u32) -> u32 {
        if let Some(&block_id) = self.address_to_block.get(&address) {
            return block_id;
        }

        let block_id = self.create_new_block();
        self.address_to_block.insert(address, block_id);
        block_id
    }
}

/// Convert P-Code type to IR type
fn pcode_type_to_ir_type(pcode_type: PCodeType) -> TypeKind {
    match pcode_type {
        PCodeType::Byte => TypeKind::Byte,
        PCodeType::Boolean => TypeKind::Boolean,
        PCodeType::Integer => TypeKind::Integer,
        PCodeType::Long => TypeKind::Long,
        PCodeType::Single => TypeKind::Single,
        PCodeType::String => TypeKind::String,
        PCodeType::Object => TypeKind::Object,
        PCodeType::Variant | PCodeType::Unknown => TypeKind::Variant,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_lifter_creation() {
        let lifter = PCodeLifter::new();
        assert!(lifter.last_error.is_none());
    }

    #[test]
    fn test_lift_empty_instructions() {
        let mut lifter = PCodeLifter::new();
        let result = lifter.lift(&[], "test".to_string(), 0);
        assert!(result.is_err());
    }

    #[test]
    fn test_pcode_type_conversion() {
        assert_eq!(pcode_type_to_ir_type(PCodeType::Byte), TypeKind::Byte);
        assert_eq!(pcode_type_to_ir_type(PCodeType::Integer), TypeKind::Integer);
        assert_eq!(pcode_type_to_ir_type(PCodeType::Long), TypeKind::Long);
        assert_eq!(pcode_type_to_ir_type(PCodeType::String), TypeKind::String);
        assert_eq!(pcode_type_to_ir_type(PCodeType::Variant), TypeKind::Variant);
    }
}
