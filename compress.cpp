
#include "common.h"

//----------------------------------------------------------------------------------------------------------------------------------------------------------
class CompressedSizeCalculatorFast
{
private:
    int MAX_OFFSET = 32768;
    int MIN_LENGTH = 1;
    int MAX_LENGTH = 65536;
    int GOOD_LENGTH = 32768;

    struct Match
    {
        int offset;
        int length;
        int cost;
        Match(int o = 0, int l = 0, int c = INT_MAX) : offset(o), length(l), cost(c) {}
    };

    struct ParseNode
    {
        int cost;
        int lastOffset;
        int literalRunStart;
        ParseNode() : cost(INT_MAX), lastOffset(0), literalRunStart(-1) {}
    };

    // Hash table with position limits for efficiency
    unordered_map<uint32_t, vector<int>> hashTable;
    static const int MAX_HASH_CHAIN = 64; // Limit hash chain length

    void buildHashTable(const vector<unsigned char>& data)
    {
        hashTable.clear();
        hashTable.reserve(data.size() / 2);

        for (int i = 0; i <= (int)data.size() - 3; i++)
        {
            uint32_t hash = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8) | data[i + 2];
            auto& chain = hashTable[hash];
            chain.push_back(i);

            // Keep only recent positions to limit search time
            if (chain.size() > MAX_HASH_CHAIN) {
                chain.erase(chain.begin());
            }
        }
    }

    inline int getEliasGammaBitLength(int value) const
    {
        if (value <= 0) return 0;

        int bitLength = bit_width((unsigned int)value) - 1;
        return 2 * bitLength + 1;
        //return 2 * bit_width((unsigned int)value) - 1;
    }

    inline int getOffsetBitLength(int offset) const
    {
        int msb = (offset - 1) >> 7;
        return getEliasGammaBitLength(msb + 1) + 7;
    }

    inline int getLiteralCost(const vector<unsigned char>& data, int pos, int runStart) const
    {
        int runLength = pos - runStart + 1;
        int currentCost = 1 + getEliasGammaBitLength(runLength) + runLength * 8;
        if (runLength == 1) {
            return currentCost;
        }
        int previousCost = 1 + getEliasGammaBitLength(runLength - 1) + (runLength - 1) * 8;
        return currentCost - previousCost;
    }

    // Fast match extension with early termination
    inline int extendMatch(const vector<unsigned char>& data, int pos, int matchPos, int maxLen, int minImprovement = 0)
    {
        int length = 0;
        const unsigned char* src = &data[pos];
        const unsigned char* ref = &data[matchPos];

        // Use 8-byte chunks for faster comparison when possible
        while (length + 8 <= maxLen) {
            if (*(uint64_t*)(src + length) != *(uint64_t*)(ref + length)) {
                break;
            }
            length += 8;
        }

        // Handle remaining bytes
        while (length < maxLen && src[length] == ref[length]) {
            length++;
            // Early exit if we have enough improvement
            if (minImprovement > 0 && length >= minImprovement) {
                break;
            }
        }

        return length;
    }

    vector<Match> findMatches(const vector<unsigned char>& data, int pos, int lastOffset)
    {
        vector<Match> matches;
        matches.reserve(16); // Reduced reservation

        int maxLookback = min(pos, MAX_OFFSET);
        if ((size_t)pos >= data.size() || maxLookback == 0) {
            return matches;
        }

        int bestLength = 0;
        int bestCost = INT_MAX;

        // Check offset reuse first
        if (lastOffset > 0 && lastOffset <= pos) {
            int matchPos = pos - lastOffset;
            int length = extendMatch(data, pos, matchPos, min(MAX_LENGTH, (int)data.size() - pos));

            if (length >= MIN_LENGTH) {
                int cost = 1 + getEliasGammaBitLength(length);
                matches.push_back(Match(-lastOffset, length, cost));
                bestLength = length;
                bestCost = cost;

                if (length >= GOOD_LENGTH) {
                    return matches; // Early exit for very good matches
                }
            }
        }

        // Hash-based search with early termination
        if ((size_t)(pos + 2) < data.size()) {
            uint32_t hash = ((uint32_t)data[pos] << 16) | ((uint32_t)data[pos + 1] << 8) | data[pos + 2];
            auto it = hashTable.find(hash);

            if (it != hashTable.end()) {
                int matchesFound = 0;
                const int MAX_MATCHES_TO_CHECK = 16; // Limit matches checked

                for (auto rit = it->second.rbegin(); rit != it->second.rend() && matchesFound < MAX_MATCHES_TO_CHECK; ++rit) {
                    int matchPos = *rit;
                    if (matchPos >= pos) continue;

                    int offset = pos - matchPos;
                    if (offset > maxLookback) continue;

                    // Skip if we already have a much better match
                    int minLength = max(3, bestLength - 8);
                    int length = extendMatch(data, pos, matchPos, min(MAX_LENGTH, (int)data.size() - pos), minLength);

                    if (length >= minLength) {
                        int offsetCost = getOffsetBitLength(offset);
                        int totalCost = 1 + getEliasGammaBitLength(length) + offsetCost;

                        // Only add if it's competitive
                        if (totalCost < bestCost || (totalCost == bestCost && length > bestLength)) {
                            matches.push_back(Match(offset, length, totalCost));
                            bestLength = max(bestLength, length);
                            bestCost = min(bestCost, totalCost);
                            matchesFound++;
                        }

                        if (length >= GOOD_LENGTH) {
                            break;
                        }
                    }
                }
            }
        }

        // Limited direct search for short offsets
        int directSearchLimit = min(min(256, maxLookback), bestLength > 0 ? bestLength + 16 : 256);

        for (int offset = 1; offset <= directSearchLimit; offset++) {
            int matchPos = pos - offset;

            // Quick rejection
            if (data[matchPos] != data[pos]) continue;

            int minLength = max(MIN_LENGTH, bestLength - 4);
            int length = extendMatch(data, pos, matchPos, min(MAX_LENGTH, (int)data.size() - pos), minLength);

            if (length >= minLength) {
                int offsetCost = getOffsetBitLength(offset);
                int totalCost = 1 + getEliasGammaBitLength(length) + offsetCost;

                if (totalCost < bestCost || (totalCost == bestCost && length > bestLength)) {
                    matches.push_back(Match(offset, length, totalCost));
                    bestLength = max(bestLength, length);
                    bestCost = min(bestCost, totalCost);
                }

                if (length >= GOOD_LENGTH) break;
            }
        }

        // Sort matches by cost/length ratio for better parsing
        sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
            double ratioA = (double)a.length / a.cost;
            double ratioB = (double)b.length / b.cost;
            return ratioA > ratioB;
            });

        // Keep only best matches to reduce DP complexity
        if (matches.size() > 8) {
            matches.resize(8);
        }

        return matches;
    }

    int calculateOptimalCost(const vector<unsigned char>& data)
    {
        int n = data.size();
        if (n == 0) return 0;

        vector<ParseNode> dp(n + 1);
        dp[0].cost = 0;
        dp[0].lastOffset = 0;
        dp[0].literalRunStart = -1;

        buildHashTable(data);

        // Process with early pruning
        for (int pos = 0; pos < n; pos++) {
            if (dp[pos].cost == INT_MAX) continue;

            // Literal option
            int literalRunStart = (dp[pos].literalRunStart == -1) ? pos : dp[pos].literalRunStart;
            int literalCost = getLiteralCost(data, pos, literalRunStart);
            int totalLiteralCost = dp[pos].cost + literalCost;

            if (totalLiteralCost < dp[pos + 1].cost) {
                dp[pos + 1].cost = totalLiteralCost;
                dp[pos + 1].lastOffset = dp[pos].lastOffset;
                dp[pos + 1].literalRunStart = literalRunStart;
            }

            // Match options
            auto matches = findMatches(data, pos, dp[pos].lastOffset);

            for (const Match& match : matches) {
                int newPos = pos + match.length;
                if (newPos > n) continue;

                int totalCost = dp[pos].cost + match.cost;

                if (totalCost < dp[newPos].cost) {
                    dp[newPos].cost = totalCost;
                    dp[newPos].literalRunStart = -1;

                    if (match.offset < 0) {
                        dp[newPos].lastOffset = dp[pos].lastOffset;
                    }
                    else {
                        dp[newPos].lastOffset = match.offset;
                    }
                }
            }

            // Skip ahead for very long matches to reduce iterations
            if (!matches.empty() && matches[0].length > 1024) {
                int skipTo = min(pos + matches[0].length / 4, n - 1);
                for (int skip = pos + 1; skip < skipTo; skip++) {
                    if (dp[skip].cost > dp[pos].cost + 100) { // Heuristic pruning
                        dp[skip].cost = INT_MAX;
                    }
                }
            }
        }

        return dp[n].cost;
    }

public:
    int calculateCompressedSize(const vector<unsigned char>& input, int max_offset = 32768, int max_length = 65536, int good_length = 32768)
    {
        if (input.empty()) return 0;

        MAX_OFFSET = max_offset;
        MAX_LENGTH = max_length;
        GOOD_LENGTH = good_length;

        int dataBits = calculateOptimalCost(input);
        int eofBits = 1 + 1 + getEliasGammaBitLength(256) + 7;
        int totalBits = dataBits + eofBits;

        return (totalBits + 7) / 8;
    }
};

class CompressedSizeCalculator
{
private:
    int MAX_OFFSET = 32768;
    int MIN_LENGTH = 1;
    int MAX_LENGTH = 65536;
    int GOOD_LENGTH = 32768;

    struct Match
    {
        int offset;
        int length;
        int cost;   //Bit cost for this match
        Match(int o = 0, int l = 0, int c = INT_MAX) : offset(o), length(l), cost(c) {}
    };

    struct ParseNode
    {
        int cost;
        int lastOffset;         //Track last used offset for context  
        int literalRunStart;    //Track start of current literal run (-1 if not in literal run)
        ParseNode() : cost(INT_MAX), lastOffset(0), literalRunStart(-1) {}
    };

    //Hash table for fast match finding
    unordered_map<uint32_t, vector<int>> hashTable;

    void buildHashTable(const vector<unsigned char>& data)
    {
        hashTable.clear();
        hashTable.reserve(data.size() / 2);  //Reserve space for efficiency
        for (int i = 0; i <= (int)data.size() - 3; i++)
        {
            uint32_t hash = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8) | data[i + 2];
            hashTable[hash].push_back(i);
        }
    }

    //Calculate Elias Gamma bit length
    inline int getEliasGammaBitLength(int value) const
    {
        if (value <= 0) return 0;

        int bitLength = bit_width((unsigned int)value) - 1;
        return 2 * bitLength + 1;
        //return 2 * bit_width((unsigned int)value) - 1;
    }

    //Calculate offset encoding cost (MSB + 7-bit LSB)
    inline int getOffsetBitLength(int offset) const
    {
        int msb = (offset - 1) >> 7;
        return getEliasGammaBitLength(msb + 1) + 7;  // MSB (Elias Gamma) + 7-bit LSB
    }

    //Calculate cost for literal at current position
    //In ZX0, literals are encoded as: 0 flag + Elias Gamma length + literal bytes
    //But for optimal parsing, we consider the marginal cost of adding one more literal
    inline int getLiteralCost(const vector<unsigned char>& data, int pos, int runStart) const
    {
        int runLength = pos - runStart + 1;

        //Cost of current run with this literal
        int currentCost = 1 + getEliasGammaBitLength(runLength) + runLength * 8;

        if (runLength == 1)
        {
            return currentCost; //First literal in run
        }

        //Marginal cost = new total cost - previous total cost
        int previousCost = 1 + getEliasGammaBitLength(runLength - 1) + (runLength - 1) * 8;
        return currentCost - previousCost;
    }

    //Find all viable matches at current position
    vector<Match> findMatches(const vector<unsigned char>& data, int pos, int lastOffset)
    {
        vector<Match> matches;
        matches.reserve(32);  //Typical number of matches to avoid reallocations
        int maxLookback = min(pos, MAX_OFFSET);

        if ((size_t)pos >= data.size() || maxLookback == 0)
        {
            return matches;
        }

        //Check offset reuse first (higher priority in ZX0)
        if (lastOffset > 0 && lastOffset <= pos)
        {
            int matchPos = pos - lastOffset;
            int length = 0;
            int maxLen = min(MAX_LENGTH, (int)data.size() - pos);

            //Fast match extension
            while (length < maxLen && data[matchPos + length] == data[pos + length])
            {
                length++;
            }

            if (length >= MIN_LENGTH)
            {
                int cost = 1 + getEliasGammaBitLength(length);          //1 reuse flag + length
                matches.push_back(Match(-lastOffset, length, cost));    //Negative indicates reuse
            }
        }

        //Hash-based match finding for positions with 3+ bytes
        if ((size_t)(pos + 2) < data.size())
        {            
            uint32_t hash = ((uint32_t)data[pos] << 16) | ((uint32_t)data[(size_t)(pos + 1)] << 8) | data[(size_t)(pos + 2)];
            auto it = hashTable.find(hash);
            
            if (it != hashTable.end())
            {
                for (int matchPos : it->second)
                {
                    if (matchPos >= pos)
                    {
                        break;  //Only look backwards
                    }
                    
                    int offset = pos - matchPos;
                    
                    if (offset > maxLookback)
                    {
                        continue;
                    }
                    
                    int length = 3;  //We know at least 3 bytes match from hash
                    int maxLen = min(MAX_LENGTH, (int)data.size() - pos);

                    //Extend match as far as possible
                    while (length < maxLen && data[matchPos + length] == data[pos + length])
                    {
                        length++;
                    }

                    //Calculate total encoding cost
                    int offsetCost = getOffsetBitLength(offset);
                    int totalCost = 1 + getEliasGammaBitLength(length) + offsetCost;  //new offset flag + length + offset
                    matches.push_back(Match(offset, length, totalCost));

                    if (length >= GOOD_LENGTH)
                    {
                        break;
                    }
                }
            }
        }

        //Direct search for short offsets (often more effective for small distances)
        int directSearchLimit = min(512, maxLookback);  // Limit direct search
        for (int offset = 1; offset <= directSearchLimit; offset++)
        {
            int matchPos = pos - offset;
            int length = 0;
            int maxLen = min(MAX_LENGTH, (int)data.size() - pos);

            //Fast match detection - check first few bytes quickly
            if (data[matchPos] != data[pos])
            {
                continue;
            }

            while (length < maxLen && data[matchPos + length] == data[pos + length])
            {
                length++;
            }

            if (length >= MIN_LENGTH)
            {
                int offsetCost = getOffsetBitLength(offset);
                int totalCost = 1 + getEliasGammaBitLength(length) + offsetCost;
                matches.push_back(Match(offset, length, totalCost));
            }
            if (length >= GOOD_LENGTH)
            {
                break;
            }
        }

        return matches;
    }

    //Optimal parsing using dynamic programming
    int calculateOptimalCost(const vector<unsigned char>& data)
    {
        int n = data.size();
        if (n == 0) return 0;

        vector<ParseNode> dp(n + 1);
        dp[0].cost = 0;
        dp[0].lastOffset = 0;
        dp[0].literalRunStart = -1;

        buildHashTable(data);

        for (int pos = 0; pos < n; pos++)
        {
            if (dp[pos].cost == INT_MAX)
            {
                continue;
            }

            //Try adding current byte to literal run
            int literalRunStart = (dp[pos].literalRunStart == -1) ? pos : dp[pos].literalRunStart;
            int literalCost = getLiteralCost(data, pos, literalRunStart);
            int totalLiteralCost = dp[pos].cost + literalCost;

            if (totalLiteralCost < dp[pos + 1].cost)
            {
                dp[pos + 1].cost = totalLiteralCost;
                dp[pos + 1].lastOffset = dp[pos].lastOffset;    //Preserve last offset
                dp[pos + 1].literalRunStart = literalRunStart;  //Continue literal run
            }

            //Try all possible matches
            auto matches = findMatches(data, pos, dp[pos].lastOffset);
            for (const Match& match : matches)
            {
                int newPos = pos + match.length;
                if (newPos > n)
                {
                    continue;
                }

                int totalCost = dp[pos].cost + match.cost;
                
                if (totalCost < dp[newPos].cost)
                {
                    dp[newPos].cost = totalCost;
                    dp[newPos].literalRunStart = -1; //Match breaks literal run
                    //Update context for next decisions
                    if (match.offset < 0)
                    {
                        dp[newPos].lastOffset = dp[pos].lastOffset; //Reused offset
                    }
                    else
                    {
                        dp[newPos].lastOffset = match.offset;       //New offset becomes last
                    }
                }
            }
        }

        return dp[n].cost;
    }

public:
    //Calculate compressed size in bytes
    int calculateCompressedSize(const vector<unsigned char>& input, int max_offset = 32768, int max_length = 65536, int good_length = 32768)
    {
        if (input.empty())
        {
            return 0;
        }

        MAX_OFFSET = max_offset;
        MAX_LENGTH = max_length;
        GOOD_LENGTH = good_length;

        //Get optimal compression cost in bits
        int dataBits = calculateOptimalCost(input);

        //Add EOF marker cost: 1 copy flag + 1 new offset flag + Elias Gamma(256) + 7 LSB bits
        int eofBits = 1 + 1 + getEliasGammaBitLength(256) + 7;
        int totalBits = dataBits + eofBits;

        //Convert to bytes (round up)
        return (totalBits + 7) / 8;
    }
};

//----------------------------------------------------------------------------------------------------------------------------------------------------------

int CalculateCompressedSize(const vector<unsigned char>& data)
{    
    int NumLongRuns = 0;
    int RunLength = 0;
    int LongRunLength = 0;

    unsigned char ThisChar = data[0] ^ 0xff;
    for (size_t i = 0; i < data.size(); i++)
    {
        if (data[i] == ThisChar)
        {
            RunLength++;
            if (RunLength == 128)
            {
                NumLongRuns++;
            }
        }
        else
        {
            if (RunLength > 127)
            {
                LongRunLength += RunLength;
            }
            ThisChar = data[i];
            RunLength = 0;
        }
    }

    int result = 0;

    if (NumLongRuns > 16 || LongRunLength > 0x1000 || data.size() > 32768)
    {
        CompressedSizeCalculatorFast calculator;
        result = calculator.calculateCompressedSize(data, 32768, 65536, 32);
    }
    else
    {
        CompressedSizeCalculator calculator;
        result = calculator.calculateCompressedSize(data, 32768, 65536, 32);
    }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------
