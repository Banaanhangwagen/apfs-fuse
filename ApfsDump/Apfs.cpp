/*
	This file is part of apfs-fuse, a read-only implementation of APFS
	(Apple File System) for FUSE.
	Copyright (C) 2017 Simon Gander

	Apfs-fuse is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	Apfs-fuse is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with apfs-fuse.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <fstream>
#include <iostream>
#include <iomanip>

#include <ApfsLib/Device.h>
#include <ApfsLib/Util.h>
#include <ApfsLib/DiskStruct.h>
#include <ApfsLib/BlockDumper.h>
#include <ApfsLib/GptPartitionMap.h>

#ifdef __linux__
#include <signal.h>
#endif

#undef RAW_VERBOSE

constexpr size_t BLOCKSIZE = 0x1000;

volatile bool g_abort = 0;

void DumpBlockTrunc(std::ostream &os, const byte_t *data)
{
	unsigned int sz = BLOCKSIZE - 1;

	while (sz > 0 && data[sz] == 0)
		sz--;

	sz = (sz + 0x10) & 0xFFFFFFF0;

	DumpHex(os, data, sz);
}

void MapBlocks(std::ostream &os, Device &dev, uint64_t bid_start, uint64_t bcnt)
{
	using namespace std;

	uint64_t bid;
	uint8_t block[BLOCKSIZE];
	const APFS_BlockHeader * const blk = reinterpret_cast<const APFS_BlockHeader *>(block);
	const APFS_TableHeader * const tbl = reinterpret_cast<const APFS_TableHeader *>(block + sizeof(APFS_BlockHeader));
	bool last_was_used = false;

	os << hex << uppercase << setfill('0');

	os << "[Block]  | Node ID  | Version  | Type     | Subtype  | Flgs | Levl | Entries  | Description" << endl;
	os << "---------+----------+----------+----------+----------+------+------+----------+---------------------------------" << endl;

	for (bid = 0; bid < bcnt && !g_abort; bid++)
	{
		dev.Read(block, (bid_start + bid) * BLOCKSIZE, BLOCKSIZE);

		if (IsEmptyBlock(block, BLOCKSIZE))
		{
			if (last_was_used)
				os << "---------+----------+----------+----------+----------+------+------+----------+ Empty" << endl;
			last_was_used = false;
			continue;
		}

		if (VerifyBlock(block, BLOCKSIZE))
		{
			os << setw(8) << bid << " | ";
			os << setw(8) << blk->nid << " | ";
			os << setw(8) << blk->xid << " | ";
			os << setw(8) << blk->type << " | ";
			os << setw(8) << blk->subtype << " | ";
			os << setw(4) << tbl->page << " | ";
			os << setw(4) << tbl->level << " | ";
			os << setw(8) << tbl->entries_cnt << " | ";
			os << BlockDumper::GetNodeType(blk->type, blk->subtype);
			if ((blk->type & 0xFFFFFFF) == 2)
				os << " [Root]";
			os << endl;
			last_was_used = true;
		}
		else
		{
			os << setw(8) << bid;
			os << " |          |          |          |          |      |      |          | Data" << endl;
			last_was_used = true;
		}
	}

	os << endl;
}

void ScanBlocks(std::ostream &os, Device &dev, uint64_t bid_start, uint64_t bcnt)
{
	BlockDumper bd(os, BLOCKSIZE);
	uint64_t bid;
	uint8_t block[BLOCKSIZE];

	bd.SetTextFlags(0x01);

	for (bid = 0; bid < bcnt && !g_abort; bid++)
	{
		dev.Read(block, (bid_start + bid) * BLOCKSIZE, BLOCKSIZE);

		if (IsEmptyBlock(block, BLOCKSIZE))
			continue;

		if (VerifyBlock(block, BLOCKSIZE))
			bd.DumpNode(block, bid);
		else
		{
#if 0
			os << std::hex << std::setw(16) << blk_nr << std::endl;
			DumpBlockTrunc(os, block);
			os << std::endl;
			os << "========================================================================================================================" << std::endl;
			os << std::endl;
#endif
		}
	}
}

static void ctrl_c_handler(int sig)
{
	(void)sig;
	g_abort = true;
}

int main(int argc, const char *argv[])
{
	if (argc < 3)
	{
		std::cerr << "Syntax: apfs-dump file.img output.txt [map.txt]" << std::endl;
		return 1;
	}

	Device *dev;
	std::ofstream os;
	uint64_t bid_start = 0;
	uint64_t bcnt = 0;

	g_debug = 16;

#if defined(__linux__) || defined(__APPLE__)
	signal(SIGINT, ctrl_c_handler);
#endif

	dev = Device::OpenDevice(argv[1]);

	if (!dev)
	{
		std::cerr << "Device " << argv[1] << " not found." << std::endl;
		return 2;
	}

	{
		GptPartitionMap pmap;
		if (pmap.LoadAndVerify(*dev))
		{
			int partid = pmap.FindFirstAPFSPartition();
			if (partid >= 0)
			{
				pmap.GetPartitionOffsetAndSize(partid, bid_start, bcnt);
				bid_start /= BLOCKSIZE;
				bcnt /= BLOCKSIZE;
			}
		}
	}

	if (bcnt == 0)
		bcnt = dev->GetSize() / BLOCKSIZE;

	if (argc > 3)
	{
		os.open(argv[3]);
		if (!os.is_open())
		{
			std::cerr << "Could not open output file " << argv[3] << std::endl;
			dev->Close();
			delete dev;
			return 3;
		}

		MapBlocks(os, *dev, bid_start, bcnt);
		os.close();
	}

	os.open(argv[2]);
	if (!os.is_open())
	{
		std::cerr << "Could not open output file " << argv[2] << std::endl;
		dev->Close();
		delete dev;
		return 3;
	}

	ScanBlocks(os, *dev, bid_start, bcnt);

	dev->Close();
	os.close();

	delete dev;

	return 0;
}
