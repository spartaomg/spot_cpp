
#include "common.h"

const int rows = 16;
const int cols = 16;

int NumCovered = 0;

double matrix[rows][cols]{};
int amatrix[rows][cols]{};
int pmatrix[rows][cols]{};

int CoveredRows[rows]{};
int CoveredCols[cols]{};

int PaletteAssignment[16]{};

//------------------------------------------------------------------------------------------------------------------------------------------------------------

/*

-> assign the first 0 in each row that is not in a covered/assigned column and cover/assign its column
	-> also cover each assigned col, uncover each row

->find minimum number of lines to cover all 0s in the matirx

while (NumCovered < rows)

	int r = 0;
	int c = 0;

	Flipped0 = false
	Found0 = true

	while ((Found0) && (!Flipped0))
		Found0 = false
		for (c = 0 to cols)
			if (CoveredCol[c]==0)
				for (r = 0 to rows)
					if (CoveredRow[r] == 0)
						if (matrix[r][c]==0)
							pmatrix[r][c]=1;	//Prime uncovered 0
							PrimedCol[r]=c;		//Save primed row and col indices
							PrimedRow[c]=r;
							//Primed 0s are either in the same row or the same col of an assigned 0
							
							if (AssignedCol[r] > -1)		//Reverse use of AssignedCol[r] (assigned col of row) and AssignedRow[c] (assigned row of col)
								//this row does have an assigned 0, in col AssignedCol[r]
								//uncover the assigned 0's column and cover this row
								//then continue looking for an uncovered 0 until no more found
								CoveredRow[r]=1
								CoveredCol[AssignedCol[r]=0;
								Found0=true
								break
							else
								//this row does not have an assigned 0
								//so this column must have one
								-> flip primed and assigned 0s
									(Also update amatrix, delete pmatrix, uncover all rows, cover assigned columens)
								Flipped0=true
								break;
								//We will exit both [c] and [r] for loops and the ((Found0) && (!Flipped0) while loop too
							end if
						end if
					end if
				next r
			end if
			//We exit both for loops if (Flipped0), but only the inner loop if (Found0)
			if (Flipped0)
				break;
		next c
	end while ((Found0) && (!Flipped0))

	if (!Flipped0)
		//we ran out of unassinged 0's (without flipping, i.e. we didn't change the number of 0s and covered lines)
		//we need to create more 0s
		CreateNew0()
		->create more 0s
		->uncover all lines, clear amatrix and pmatrix
		->assign 0s and cover columns, calculate NumCovered
	end if (!Flipped0)

end while (NumCovered)

*/

//------------------------------------------------------------------------------------------------------------------------------------------------------------
/*
void ShowMatrix()
{
	cout << "\n";

	for (int c = 0; c < cols; c++)
	{
		cout << "\t" << c;
	}
	cout << "\n";

	for (int r = 0; r < rows; r++)
	{
		cout << r;
		for (int c = 0; c < cols; c++)
		{
			if (matrix[r][c] > 0)
			{
				cout << "\t" << matrix[r][c];
			}
			else if (amatrix[r][c] == 1)
			{
				cout << "\t0*";
			}
			else if (matrix[r][c] == 1)
			{
				cout << "\t0'";
			}
			else
			{
				cout << "\t0";
			}
		}
		cout << "\n";
	}

	cout << "\n\n";

}
*/
//------------------------------------------------------------------------------------------------------------------------------------------------------------

void UncoverLines()
{

	for (int r = 0; r < rows; r++)
	{
		CoveredRows[r] = 0;

		for (int c = 0; c < cols; c++)
		{
			amatrix[r][c] = 0;
			pmatrix[r][c] = 0;
		}
	}

	//for (int r = 0; r < rows; r++)
	//{
		//CoveredRows[r] = 0;
	//}

	for (int c = 0; c < cols; c++)
	{
		CoveredCols[c] = 0;
	}

	NumCovered = 0;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------

void AssignZerosAndCoverColumns()
{
	//Assign the first 0s in each row that are not in a covered column
	//Save assigned indices
	//Calculate NumCovered

	UncoverLines();

	for (int r = 0; r < rows; r++)
	{
		for (int c = 0; c < cols; c++)
		{
			if (CoveredCols[c] == 0)
			{
				if (matrix[r][c] == 0)
				{
					NumCovered++;			//Count covered lines

					CoveredCols[c] = 1;		//Build CoveredCols array
					//CoveredRows[r] = 1;
					amatrix[r][c] = 1;		//Build assigned matrix

					break;
				}
			}
		}
	}

	//for (int r = 0; r < rows; r++)		//Not needed if we don't set CoveredRows values to 1 above...
		//CoveredRows[r] = 0;

	//ShowMatrix();
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------

void CreateNewZeros()
{
	//Find smallest uncovered element in matrix
	//Subtract it from all uncovered elements
	//Add it to all double-covered elements
	
	double MinVal = numeric_limits<double>::max();

	for (int r = 0; r < rows; r++)
		if (CoveredRows[r] == 0)
			for (int c = 0; c < cols; c++)
				if (CoveredCols[c] == 0)
				{
					MinVal = min(matrix[r][c], MinVal);
					//if (MinVal > matrix[r][c])
					//{
						//MinVal = matrix[r][c];
					//}
				}

	for (int r = 0; r < rows; r++)
	{
		for (int c = 0; c < cols; c++)
		{
			if ((CoveredRows[r] == 0) && (CoveredCols[c] == 0))
			{
				matrix[r][c] -= MinVal;
			}
			else if ((CoveredRows[r] == 1) && (CoveredCols[c] == 1))
			{
				matrix[r][c] += MinVal;
			}
		}
	}

	//Uncover lines, create assigned matrix, cover columns, calculate NumCovered, create assigned indices

	AssignZerosAndCoverColumns();
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------

void FlipZeros(int pr, int pc)
{
	int ac = pc;						//the original assigned 0 is on the same column
	int ar = 0;
	
	for (ar = 0; ar < rows; ar++)		//find its row
		if (amatrix[ar][ac] == 1)
			break;

	amatrix[pr][pc] = 1;				//assign primed 0

	while (ar < rows)
	{
		//find primed 0 - there always must be one

		pr = ar;						//same row as the original assigned 0 (now un-assigned)
		for (pc = 0; pc < cols; pc++)
			if (pmatrix[pr][pc] == 1)
				break;

		amatrix[ar][ac] = 0;			//unassign assigned 0


		//find assigned 0 in current column

		ac = pc;
		for (ar = 0; ar < rows; ar++)
			if (amatrix[ar][ac] == 1)
				break;

		//assign primed 0

		amatrix[pr][pc] = 1;		//assign this 0
	}

	//Update amatrix, delete pmatrix, uncover all rows, cover assigned columens

	NumCovered = 0;
	for (ar = 0; ar < rows; ar++)
	{
		CoveredRows[ar] = 0;
		for (ac = 0; ac < cols; ac++)
		{
			if (amatrix[ar][ac] == 1)
			{
				CoveredCols[ac] = 1;
				NumCovered++;
			}
			pmatrix[ar][ac] = 0;
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------

void CoverLines()
{
	while (NumCovered < cols)
	{
		bool Found0 = true;
		bool Flipped0 = false;

		while ((Found0) && (!Flipped0))
		{
			Found0 = false;
			
			for (int c = 0; c < cols; c++)
			{
				if (CoveredCols[c] == 0)
				{
					for (int r = 0; r < rows; r++)
					{
						if (CoveredRows[r] == 0)
						{
							if (matrix[r][c] == 0)
							{
								pmatrix[r][c] = 1;		//Uncovered 0 found, prime it

								//Primed 0s are either in the same row or the same col of an assigned 0
								int ac = 0;
								for (ac = 0; ac < cols; ac++)
									if (amatrix[r][ac] == 1)
										break;

								if (ac < cols)
								{
									//this row does have an assigned 0, in col AssignedCol[r]
									//uncover the assigned 0's column and cover this row
									//then continue looking for an uncovered 0 until no more found
									CoveredRows[r] = 1;
									CoveredCols[ac] = 0;
									Found0 = true;
									break;
								}
								else
								{
									//this row does not have an assigned 0
									//so this column must have one
									FlipZeros(r, c);
									//-> flip primed and assigned 0s
									//-> update amatrix, delete pmatrix, uncover all rows, cover assigned columens)
									Flipped0 = true;
									break;
									//We will exit both [c] and [r] for loops and the ((Found0) && (!Flipped0) while loop too
								}
							}
						}
					}
				}
				//We exit both for loops if (Flipped0), but only the inner loop if (Found0)
				if (Flipped0)
					break;
			}
		}

		if (!Flipped0)
		{
			//we ran out of unassinged 0's (without flipping, i.e. we didn't change the number of 0s and covered lines)
			//we need to create more 0s
			CreateNewZeros();
			//->create more 0s
			//->uncover all lines, clear amatrix and pmatrix
			//->assign 0s and cover columns, calculate NumCovered
		}	//end if (!Flipped0)
	}
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------

void FindOptimalAssignment()
{
	//INITIAL STEPS

	//For each row in the matrix, find the smallest element and subtract it from every element in the row.
	//If there is at least one 0 in each column then we are done

	for (int r = 0; r < rows; r++)
	{
		double MinVal = matrix[r][0];
		for (int c = 1; c < cols; c++)
		{
			MinVal = min(MinVal, matrix[r][c]);
		}
		
		for (int c = 0; c < cols; c++)
		{
			matrix[r][c] -= MinVal;
			if (matrix[r][c] == 0)
			{
				if (CoveredCols[c] == 0)
				{
					CoveredCols[c] = 1;		//Cover column
					NumCovered++;			//Also count covered columns
				}
			}
		}
	}

	if (NumCovered == cols)		//If all column is covered then we are done
	{
		AssignZerosAndCoverColumns();
		return;
	}

	//Subtract the smallest value in each column

	for (int c = 0; c < cols; c++)
	{
		if (CoveredCols[c] == 0)			//Only uncovered columns will have a non-0 min value
		{
			double MinVal = matrix[0][c];
			for (int r = 1; r < rows; r++)
			{
				MinVal = min(matrix[r][c], MinVal);
			}

			for (int r = 0; r < rows; r++)
			{
				matrix[r][c] -= MinVal;
			}
		}
		else
		{
			CoveredCols[c] = 0;				//Otherwise uncover column
		}
	}

	AssignZerosAndCoverColumns();

	CoverLines();
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------

double HungarianAlgorithm(double m[16][16])
{
	//Create copy of m matrix and reset amatrix and pmatrix
	
	for (int r = 0; r < rows; r++)
	{
		for (int c = 0; c < cols; c++)
		{
			matrix[r][c] = m[r][c];
		}
	}

	UncoverLines();				//Also resets Numcovered, and clears amatrix and pmatirx

	FindOptimalAssignment();
	
	double cost = 0.0;
	
	for (int r = 0; r < rows; r++)
	{
		for (int c = 0; c < cols; c++)
		{
			if (amatrix[r][c] == 1)
			{
				PaletteAssignment[c] = r;
				cost += m[r][c];
			}
		}
	}

	return cost;

}