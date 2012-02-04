//
// The Epoch Language Project
// EPOCHCOMPILER Compiler Toolchain
//
// IR classes for representing expressions
//

#pragma once


// Dependencies
#include "Compiler/ExportDef.h"

#include "Utility/Types/IDTypes.h"
#include "Utility/Types/IntegerTypes.h"
#include "Utility/Types/RealTypes.h"
#include "Utility/Types/EpochTypeIDs.h"

#include <vector>


namespace IRSemantics
{

	// Forward declarations
	class Statement;
	class PreOpStatement;
	class PostOpStatement;
	class CodeBlock;
	class Program;
	struct InferenceContext;


	class ExpressionAtom
	{
	// Destruction
	public:
		virtual ~ExpressionAtom()
		{ }

	// Atom interface
	public:
		virtual VM::EpochTypeID GetEpochType(const Program& program) const = 0;
		virtual bool TypeInference(Program& program, CodeBlock& activescope, InferenceContext& context, size_t index) = 0;
	};


	class Expression
	{
	// Construction and destruction
	public:
		Expression();
		~Expression();

	// Type system
	public:
		EPOCHCOMPILER VM::EpochTypeID GetEpochType(const Program& program) const;

	// Validation
	public:
		bool Validate(const Program& program) const;

	// Compile time code execution
	public:
		bool CompileTimeCodeExecution(Program& program, CodeBlock& activescope);

	// Type inference
	public:
		bool TypeInference(Program& program, CodeBlock& activescope, InferenceContext& context, size_t index);

	// Atom access and manipulation
	public:
		void AddAtom(ExpressionAtom* atom);

		const std::vector<ExpressionAtom*>& GetAtoms() const
		{ return Atoms; }

	// Internal helpers
	private:
		void Coalesce(Program& program, CodeBlock& activescope);

	// Internal state
	private:
		std::vector<ExpressionAtom*> Atoms;
		VM::EpochTypeID InferredType;
	};


	class Parenthetical
	{
	// Destruction
	public:
		virtual ~Parenthetical()
		{ }
	};


	class ParentheticalPreOp : public Parenthetical
	{
	// Construction and destruction
	public:
		explicit ParentheticalPreOp(PreOpStatement* statement);
		virtual ~ParentheticalPreOp();

	// Non-copyable
	private:
		ParentheticalPreOp(const ParentheticalPreOp& other);
		ParentheticalPreOp& operator = (const ParentheticalPreOp& rhs);

	// Internal state
	private:
		PreOpStatement* MyStatement;
	};

	class ParentheticalPostOp : public Parenthetical
	{
	// Construction and destruction
	public:
		explicit ParentheticalPostOp(PostOpStatement* statement);
		virtual ~ParentheticalPostOp();

	// Non-copyable
	private:
		ParentheticalPostOp(const ParentheticalPostOp& other);
		ParentheticalPostOp& operator = (const ParentheticalPostOp& rhs);

	// Internal state
	private:
		PostOpStatement* MyStatement;
	};


	class ExpressionAtomParenthetical : public ExpressionAtom
	{
	// Construction and destruction
	public:
		explicit ExpressionAtomParenthetical(Parenthetical* parenthetical);
		virtual ~ExpressionAtomParenthetical();

	// Non-copyable
	private:
		ExpressionAtomParenthetical(const ExpressionAtomParenthetical& other);
		ExpressionAtomParenthetical& operator = (const ExpressionAtomParenthetical& rhs);

	// Atom interface
	public:
		virtual VM::EpochTypeID GetEpochType(const Program& program) const;
		virtual bool TypeInference(Program& program, CodeBlock& activescope, InferenceContext& context, size_t index);

	// Internal state
	private:
		Parenthetical* MyParenthetical;
	};

	class ExpressionAtomIdentifier : public ExpressionAtom
	{
	// Construction
	public:
		explicit ExpressionAtomIdentifier(StringHandle identifier)
			: Identifier(identifier)
		{ }

	// Accessors
	public:
		StringHandle GetIdentifier() const
		{ return Identifier; }

	// Atom interface
	public:
		virtual VM::EpochTypeID GetEpochType(const Program& program) const;
		virtual bool TypeInference(Program& program, CodeBlock& activescope, InferenceContext& context, size_t index);

	// Internal state
	private:
		StringHandle Identifier;
	};

	class ExpressionAtomOperator : public ExpressionAtom
	{
	// Construction
	public:
		explicit ExpressionAtomOperator(StringHandle identifier)
			: Identifier(identifier)
		{ }

	// Accessors
	public:
		StringHandle GetIdentifier() const
		{ return Identifier; }

	// Atom interface
	public:
		virtual VM::EpochTypeID GetEpochType(const Program& program) const;
		virtual bool TypeInference(Program& program, CodeBlock& activescope, InferenceContext& context, size_t index);

	// Internal state
	private:
		StringHandle Identifier;
	};

	class ExpressionAtomLiteralString : public ExpressionAtom
	{
	// Construction
	public:
		explicit ExpressionAtomLiteralString(StringHandle handle)
			: Handle(handle)
		{ }

	// Accessors
	public:
		StringHandle GetValue() const
		{ return Handle; }

	// Atom interface
	public:
		virtual VM::EpochTypeID GetEpochType(const Program& program) const;
		virtual bool TypeInference(Program& program, CodeBlock& activescope, InferenceContext& context, size_t index);

	// Internal state
	private:
		StringHandle Handle;
	};

	class ExpressionAtomLiteralBoolean : public ExpressionAtom
	{
	// Construction
	public:
		explicit ExpressionAtomLiteralBoolean(bool value)
			: Value(value)
		{ }

	// Accessors
	public:
		bool GetValue() const
		{ return Value; }

	// Atom interface
	public:
		virtual VM::EpochTypeID GetEpochType(const Program& program) const;
		virtual bool TypeInference(Program& program, CodeBlock& activescope, InferenceContext& context, size_t index);

	// Internal state
	private:
		bool Value;
	};

	class ExpressionAtomLiteralInteger32 : public ExpressionAtom
	{
	// Construction
	public:
		explicit ExpressionAtomLiteralInteger32(Integer32 value)
			: Value(value)
		{ }

	// Accessors
	public:
		Integer32 GetValue() const
		{ return Value; }

	// Atom interface
	public:
		virtual VM::EpochTypeID GetEpochType(const Program& program) const;
		virtual bool TypeInference(Program& program, CodeBlock& activescope, InferenceContext& context, size_t index);

	// Internal state
	private:
		Integer32 Value;
	};

	class ExpressionAtomLiteralReal32 : public ExpressionAtom
	{
	// Construction
	public:
		explicit ExpressionAtomLiteralReal32(Real32 value)
			: Value(value)
		{ }

	// Accessors
	public:
		Real32 GetValue() const
		{ return Value; }

	// Atom interface
	public:
		virtual VM::EpochTypeID GetEpochType(const Program& program) const;
		virtual bool TypeInference(Program& program, CodeBlock& activescope, InferenceContext& context, size_t index);

	// Internal state
	private:
		Real32 Value;
	};

	class ExpressionAtomStatement : public ExpressionAtom
	{
	// Construction and destruction
	public:
		explicit ExpressionAtomStatement(Statement* statement);
		virtual ~ExpressionAtomStatement();

	// Non-copyable
	private:
		ExpressionAtomStatement(const ExpressionAtomStatement& other);
		ExpressionAtomStatement& operator = (const ExpressionAtomStatement& rhs);

	// Atom interface
	public:
		virtual VM::EpochTypeID GetEpochType(const Program& program) const;
		virtual bool TypeInference(Program& program, CodeBlock& activescope, InferenceContext& context, size_t index);

	// Accessors
	public:
		Statement& GetStatement() const
		{ return *MyStatement; }

	// Internal state
	private:
		Statement* MyStatement;
	};

}

