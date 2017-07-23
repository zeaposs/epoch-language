﻿using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace EpochVSIX.Parser
{
    class LexSession
    {

        private enum CharacterClass
        {
            White,
            Comment,
            Identifier,
            Punctuation,
            PunctuationCompound,
            Literal,
            StringLiteral,
        };

        private string Buffer;
        private List<Token> TokenCache;

        private int LexIndex;
        private int LastTokenStart;

        private int LastLineIndex;
        private int CurrentLineIndex;
        private int CurrentColumnIndex;

        private CharacterClass LexState = CharacterClass.White;
        private CharacterClass PreviousState = CharacterClass.White;


        public LexSession(string buffer)
        {
            Buffer = buffer;
            TokenCache = new List<Token>();

            LexIndex = 0;
            LastTokenStart = 0;
            LastLineIndex = 0;
            CurrentLineIndex = 0;
            CurrentColumnIndex = 0;
        }


        public bool Empty
        {
            get { return (Buffer == null) || (LexIndex >= Buffer.Length); }
        }


        public Token PeekToken(int offset)
        {
            while (TokenCache.Count <= offset)
                LexAdditionalToken();

            return TokenCache[offset];
        }

        public void ConsumeTokens(int numtokens)
        {
            while (TokenCache.Count < numtokens)
                LexAdditionalToken();

            TokenCache.RemoveRange(0, numtokens);
        }


        private void LexAdditionalToken()
        {
            int startcount = TokenCache.Count;

            while (!Empty)
            {
                char c = Buffer[LexIndex];

                if (LexState == CharacterClass.White)
                {
                    if ((c == '/') && (Buffer[LexIndex + 1] == '/'))
                    {
                        LexState = CharacterClass.Comment;
                    }
                    else if (!char.IsWhiteSpace(c))
                    {
                        LexState = LexerClassify(c, LexState);
                        LastTokenStart = LexIndex;
                    }
                    else if (c == '\n')
                    {
                        ++CurrentLineIndex;
                        CurrentColumnIndex = 0;
                    }
                }
                else if (LexState == CharacterClass.Identifier)
                {
                    bool notidentifier = false;
                    if (char.IsWhiteSpace(c))
                    {
                        notidentifier = true;
                        LexState = CharacterClass.White;
                    }
                    else if (LexerClassify(c, LexState) != CharacterClass.Identifier)
                    {
                        notidentifier = true;
                        LexState = LexerClassify(c, LexState);
                    }

                    if (notidentifier)
                        TokenCache.Add(new Token { Text = Buffer.Substring(LastTokenStart, LexIndex - LastTokenStart), Line = CurrentLineIndex, Column = CurrentColumnIndex });
                }
                else if (LexState == CharacterClass.Punctuation)
                {
                    if (char.IsWhiteSpace(c))
                        LexState = CharacterClass.White;
                    else if (LexerClassify(c, LexState) != CharacterClass.Punctuation)
                        LexState = LexerClassify(c, LexState);

                    TokenCache.Add(new Token { Text = Buffer.Substring(LastTokenStart, LexIndex - LastTokenStart), Line = CurrentLineIndex, Column = CurrentColumnIndex });
                    LastTokenStart = LexIndex;
                }
                else if (LexState == CharacterClass.PunctuationCompound)
                {
                    bool notcompound = false;
                    if (char.IsWhiteSpace(c))
                    {
                        notcompound = true;
                        LexState = CharacterClass.White;
                    }
                    else if (LexerClassify(c, LexState) != CharacterClass.PunctuationCompound)
                    {
                        notcompound = true;
                        LexState = LexerClassify(c, LexState);
                    }
                    else
                    {
                        if ((LexIndex - LastTokenStart) > 1)
                        {
                            string potentialtoken = Buffer.Substring(LastTokenStart, LexIndex - LastTokenStart);
                            if (!IsValidPunctuation(potentialtoken))
                            {
                                TokenCache.Add(new Token { Text = potentialtoken.Substring(0, potentialtoken.Length - 1), Line = CurrentLineIndex, Column = CurrentColumnIndex });
                                LastTokenStart = LexIndex - 1;
                            }
                        }
                    }

                    if (notcompound)
                    {
                        if ((LexIndex - LastTokenStart) > 1)
                        {
                            string potentialtoken = Buffer.Substring(LastTokenStart, LexIndex - LastTokenStart);
                            if (!IsValidPunctuation(potentialtoken))
                            {
                                TokenCache.Add(new Token { Text = potentialtoken.Substring(0, potentialtoken.Length - 1), Line = CurrentLineIndex, Column = CurrentColumnIndex });
                                LastTokenStart = LexIndex - 1;
                            }
                        }

                        TokenCache.Add(new Token { Text = Buffer.Substring(LastTokenStart, LexIndex - LastTokenStart), Line = CurrentLineIndex, Column = CurrentColumnIndex });
                    }
                }
                else if (LexState == CharacterClass.Comment)
                {
                    if (c == '\r')
                        LexState = CharacterClass.White;
                    else if (c == '\n')
                    {
                        LexState = CharacterClass.White;
                        ++CurrentLineIndex;
                        CurrentColumnIndex = 0;
                    }
                }
                else if (LexState == CharacterClass.StringLiteral)
                {
                    if (c == '\"')
                    {
                        LexState = CharacterClass.White;
                        TokenCache.Add(new Token { Text = Buffer.Substring(LastTokenStart, LexIndex - LastTokenStart + 1), Line = CurrentLineIndex, Column = CurrentColumnIndex });
                    }
                }
                else if (LexState == CharacterClass.Literal)
                {
                    bool notliteral = false;
                    if (char.IsWhiteSpace(c))
                    {
                        notliteral = true;
                        LexState = CharacterClass.White;
                    }
                    else if (LexerClassify(c, LexState) != CharacterClass.Literal)
                    {
                        notliteral = true;
                        LexState = LexerClassify(c, LexState);
                    }

                    if (notliteral)
                        TokenCache.Add(new Token { Text = Buffer.Substring(LastTokenStart, LexIndex - LastTokenStart), Line = CurrentLineIndex, Column = CurrentColumnIndex });
                }

                // Hack for negated literals
                if (LexState == CharacterClass.PunctuationCompound)
                {
                    if (LexerClassify(Buffer[LexIndex + 1], LexState) == CharacterClass.Literal)
                        LexState = CharacterClass.Literal;
                }

                if (LexState != PreviousState)
                    LastTokenStart = LexIndex;

                PreviousState = LexState;
                ++LexIndex;

                if (CurrentLineIndex == LastLineIndex)
                    ++CurrentColumnIndex;

                LastLineIndex = CurrentLineIndex;

                if (TokenCache.Count != startcount)
                    return;
            }

            if ((LastTokenStart < Buffer.Length) && (LexState != CharacterClass.White))
                TokenCache.Add(new Token { Text = Buffer.Substring(LastTokenStart, Buffer.Length - LastTokenStart), Line = CurrentLineIndex, Column = CurrentColumnIndex });

            if ((TokenCache.Count == startcount) && Empty)
            {
                TokenCache.Add(null);
            }
        }

        private CharacterClass LexerClassify(char c, CharacterClass currentclass)
        {
            if ("abcdefx".Contains(c))
            {
                if (currentclass == CharacterClass.Literal)
                    return CharacterClass.Literal;

                return CharacterClass.Identifier;
            }

            if ("0123456789".Contains(c))
            {
                if (currentclass == CharacterClass.Identifier)
                    return CharacterClass.Identifier;

                return CharacterClass.Literal;
            }

            if ("{}:(),;[]".Contains(c))
                return CharacterClass.Punctuation;

            if ("=&+-<>!".Contains(c))
                return CharacterClass.PunctuationCompound;

            if (c == '\"')
                return CharacterClass.StringLiteral;

            if (c == '.')
            {
                if (currentclass == CharacterClass.Literal)
                    return CharacterClass.Literal;

                if (currentclass == CharacterClass.PunctuationCompound)
                    return CharacterClass.PunctuationCompound;              // minor hack

                return CharacterClass.Punctuation;
            }

            return CharacterClass.Identifier;
        }

        private bool IsValidPunctuation(string token)
        {
            if (token == "==")
                return true;

            if (token == "!=")
                return true;

            if (token == "++")
                return true;

            if (token == "--")
                return true;

            if (token == "->")
                return true;

            if (token == "&&")
                return true;

            if (token == "+=")
                return true;

            if (token == "-=")
                return true;

            if (token == "=>")
                return true;

            return false;
        }
    }
}
