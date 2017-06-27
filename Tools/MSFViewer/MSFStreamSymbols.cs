﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace MSFViewer
{
    class MSFStreamSymbols : MSFStream
    {
        public MSFStreamSymbols(MSFStream rawstream, int index)
            : base(rawstream.GetFlattenedBuffer())
        {
            Name = $"DBI Symbols ({index})";

            ParseAllSymbols();
        }


        private class Symbol
        {
            public TypedByteSequence<ushort> Size;
            public TypedByteSequence<ushort> Type;

            public virtual void PopulateAnalysis(List<ListViewItem> lvw, AnalysisGroup group, TreeView tvw)
            {
            }
        }

        private class SymbolPublic : Symbol
        {
            public TypedByteSequence<int> PublicType;
            public TypedByteSequence<int> Offset;
            public TypedByteSequence<ushort> SectionIndex;
            public TypedByteSequence<string> Name;

            public SymbolPublic(MSFStreamSymbols stream, TypedByteSequence<ushort> size, TypedByteSequence<ushort> type)
            {
                Size = size;
                Type = type;

                PublicType = stream.ExtractInt32();
                Offset = stream.ExtractInt32();
                SectionIndex = stream.ExtractUInt16();
                Name = stream.ExtractTerminatedString();

                stream.Extract4ByteAlignment();
            }

            public override void PopulateAnalysis(List<ListViewItem> lvw, AnalysisGroup group, TreeView tvw)
            {
                AddAnalysisItem(lvw, tvw, "Public symbol type", group, PublicType);
                AddAnalysisItem(lvw, tvw, "Start offset", group, Offset);
                AddAnalysisItem(lvw, tvw, "Section index", group, SectionIndex);
                AddAnalysisItem(lvw, tvw, "Name", group, Name);
            }
        }

        private class SymbolUDT : Symbol
        {
            public TypedByteSequence<int> TypeIndex;
            public TypedByteSequence<string> Name;

            public SymbolUDT(MSFStreamSymbols stream, TypedByteSequence<ushort> size, TypedByteSequence<ushort> type)
            {
                Size = size;
                Type = type;

                TypeIndex = stream.ExtractInt32();
                Name = stream.ExtractTerminatedString();

                stream.Extract4ByteAlignment();
            }

            public override void PopulateAnalysis(List<ListViewItem> lvw, AnalysisGroup group, TreeView tvw)
            {
                AddAnalysisItem(lvw, tvw, "UDT symbol type index", group, TypeIndex);
                AddAnalysisItem(lvw, tvw, "UDT name", group, Name);
            }
        }

        
        private List<Symbol> Symbols = new List<Symbol>();
        private int UnknownSymbols = 0;


        protected override void SubclassPopulateAnalysis(List<ListViewItem> lvw, ListView lvwcontrol, TreeView tvw)
        {
            var statgroup = AddAnalysisGroup(lvwcontrol, tvw, "stats", "Statistics");
            AddAnalysisItem(lvw, tvw, "Symbols of unrecognized type", statgroup, $"{UnknownSymbols}");


            var symnode = tvw.Nodes.Find("root", false)[0].Nodes.Add("symbolsall", "Symbols");


            int i = 0;
            foreach (var sym in Symbols)
            {
                var symgroup = AddAnalysisGroup(lvwcontrol, tvw, $"symbols{i}", $"Symbol {i}", symnode);
                AddAnalysisItem(lvw, tvw, "Symbol size", symgroup, sym.Size);
                AddAnalysisItem(lvw, tvw, "Symbol type", symgroup, sym.Type);

                sym.PopulateAnalysis(lvw, symgroup, tvw);

                ++i;
            }
        }

        private void ParseAllSymbols()
        {
            while (ReadOffset < FlattenedBuffer.Length)
            {
                try
                {
                    int begin = ReadOffset;
                    var symsize = ExtractUInt16();
                    var symtype = ExtractUInt16();

                    var sym = MakeSymbol(symsize, symtype);
                    Symbols.Add(sym);

                    ReadOffset = begin + symsize.ExtractedValue + 2;
                    while ((ReadOffset & 3) != 0)
                        ++ReadOffset;
                }
                catch
                {
                    break;
                }
            }
        }


        private Symbol MakeSymbol(TypedByteSequence<ushort> size, TypedByteSequence<ushort> type)
        {
            switch (type.ExtractedValue)
            {
                case 0x1108:     // S_UDT
                    return new SymbolUDT(this, size, type);

                case 0x110e:     // S_PUB32
                    return new SymbolPublic(this, size, type);
            }

            ++UnknownSymbols;
            return new Symbol { Size = size, Type = type };
        }
    }
}
