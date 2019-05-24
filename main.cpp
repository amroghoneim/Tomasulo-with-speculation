#include <iostream>
#include <string>
#include <queue>
#include <fstream>
#include <vector>


using namespace std;

#define lw_lat 2
#define sw_lat 2
#define jmp_lat 1
#define jalr_lat 1
#define ret_lat 1
#define beq_lat 1
#define add_lat 2
#define sub_lat 2
#define addi_lat 2
#define nand_lat 1
#define mult_lat 2
#define lw_op        "lw"
#define sw_op        "sw"
#define add_op      "add"
#define addi_op    "addi"
#define jmp_op      "jmp"2
#define jalr_op     "jalr"
#define ret_op      "ret"
#define beq_op      "beq"
#define sub_op      "sub"
#define nand_op    "nand"
#define mult_op    "mult"


string Opcode;
int RS1, RD, RS2_imm;

int sys_clk = 0;

bool fetched = false, committed = false;
bool branch_taken, act_branch_taken;
int beq_count = 0;
int misprediction_count = 0;
int old_address;
int instruction_count = 0;
struct instruction {
	string Op;
	int rs1;
	int rs2_imm;
	int rd;
	int pc;
	bool is_committed;
};

vector <instruction> instructions;
int address = 0;

struct Instruction_Buffer {
	bool head;//sa7?
	bool tail;
	int inst_num;
	string op;
	int rs1;
	int rs2_imm;
	int rd;
	int pc;
};

struct Reorder_Buffer {
	bool head;//sa7?
	bool tail;
	int num;
	string Type;
	int Dest;
	int Value;
	bool ready;
	bool empty;
};



string mem[64000] = { "" }; //memory supporting 128KB (64K words ,each word is 16 bits) ??

ifstream input_list;

struct Reservation_Station {
	int CC;
	string station;
	bool Busy;
	string Op;
	int Vj;
	int Vk;
	int Qj;
	int Qk;
	int A;
	int Dest;
	int result;
	int pc;
};

struct Registers {
	int Val;
	int ROB_num;
};

void parse(string str)
{
	int find_inst, find_r1, find_r2, find_rd;
	string str_rs1, str_rs2, str_rd;
	find_inst = str.find(' ');
	Opcode = str.substr(0, find_inst); /// inst 
	// rs1
	if (Op == "jmp" || Op == "ret") {
		str_rd = str.substr(find_inst, str.length());
		RS2_imm = stoi(str_rd);
	}
	else {    // rd 
		find_rd = str.find(',');
		str_rd = str[find_r1 - 1];
		RD = stoi(str_rd);
		//rs1
		str_rs1 = str[find_rd + 2];
		RS1 = stoi(str_rs1);
		//rs2_imm
		find_rs2 = str.rfind(',');
		str_rs2 = str[find_rs2 + 2];
		RS2_imm = stoi(str_rs2);
	}
}

void get_inst_data() {
	string inst;
	int inp;
	string list_dest;
	int st_addr = 0, addr = 0;
	input_list.open("file.txt");



	if (input_list.fail()) cout << "Error opening file!\n";
	else
	{
		cout << "input starting address\n";
		cin >> st_addr;
		while (!input_list.eof())
		{
			int i = 0;
			getline(input_list, inst);

			if (inst != "stop") {
				mem[st_addr++] = inst;
				parse(inst);
				instructions.push_back(instruction{ Opcode, RS1, RS2_imm, RD, i++, false });
			}
			else break;
		}
		cout << "Please Enter address and data respectively(type -1 when inputting address to stop): ";
		while (addr != -1) {
			cin >> addr >> mem[addr];
		}
	}
}


//flag first = false;
int curr_tail = 0;
int empty = 0;
bool check_empty_IB(vector<Instruction_Buffer> &inst_buffer) {
	for (int i = 0; i < 4; i++) {
		if (sys_clk != 0) {
			if (inst_buffer[i].tail == true && inst_buffer[i].head == false)
			{
				empty = i;
				return true;
			}
		}
		else return true;
	}
	return false;
}
void Fetch(vector<Instruction_Buffer> &inst_buffer, vector<Reorder_Buffer>& ROB, vector<Reservation_Station>& RS) { // elmafroud sa7
	if (address < instructions.size()) {
		instructions[address].is_committed = false;
		instruction_count++;
		if (check_empty_IB(inst_buffer)) {
			if (inst_buffer[(empty + 1) % 4].op == "")
			{
				inst_buffer[free].op = instructions[address].Op;
				inst_buffer[free].rs1 = instructions[address].rs1;
				inst_buffer[free].rs2 = instructions[address].rs2;
				inst_buffer[free].rd_imm = instructions[address].rd_imm;
				inst_buffer[free].pc = instructions[address].pc;
				inst_buffer[(free + 1) % 4].op = instructions[address].Op;
				inst_buffer[(free + 1) % 4].rs1 = instructions[address].rs1;
				inst_buffer[(free + 1) % 4].rs2 = instructions[address].rs2;
				inst_buffer[(free + 1) % 4].rd_imm = instructions[address].rd_imm;
				inst_buffer[(free + 1) % 4].pc = instructions[address].pc;
				address = address + 2;
			}
			else {
				inst_buffer[free].op = instructions[address].Op;
				inst_buffer[free].rs1 = instructions[address].rs1;
				inst_buffer[free].rs2 = instructions[address].rs2;
				inst_buffer[free].rd_imm = instructions[address].rd_imm;
				inst_buffer[free].pc = instructions[address].pc;
				address++;
			}
		}
	}
}


void issue(vector<Instruction_Buffer>& inst_buffer, vector<Reorder_Buffer>& ROB, Reservation_Station rs[], Registers rf[]) //elmafroud sa7
{
	int issue_count = 2, int inst_buffer_index;
	string opcode;
	for (int count = 0; count < issue_count; count++) {
		for (int j = 0; j < 6; j++) {
			if (ROB[j].tail == true && ROB[j].empty == false) { //check if ROB does not have an empty place
				break;
			}
			if (ROB[j].tail == true && ROB[j].empty == true) { //else
				for (int i = 0; i < 4; i++) // get instruction to be issued
				{
					if (inst_buffer[i].head == true && inst_buffer[i].op != "")
					{
						inst_buffer_index = i;
						opcode = inst_buffer[i].op;
						if (inst_buffer[i].op == "lw") // LW inst
							if (rs[0].Busy == false) {
								rs[0].CC = 2;
								rs[0].Bus
									y = true;
								rs[0].Op = "lw";
								for (int k = 0; k < 6; k++) {
									if (ROB[k].Dest == inst_buffer[i].rs1) {
										rs[0].Qj = ROB[k].num;
										rs[0].Vj = -1;
										break;
									}// if dependent on  a previous instruction
									else rs[0].Vj = rf[inst_buffer[i].rs1]; //value in register at address rs1
								}

								rs[0
								].Dest = ROB[j].num;
								rs[0].A = inst_buffer[i].rs2_imm;
								rs[0].pc = inst_buffer[i].pc;

							}
							else if (rs[1].Busy == false) {
								rs[1].CC = 2;
								rs[1].Busy = true;
								rs[1].Op = "lw";
								for (int k = 0; k < 6; k++) {
									if (ROB[k].Dest == inst_buffer[i].rs1) {
										rs[1].Qj = ROB[k].num;
										rs[1].Vj = -1;
										break;
									}// if dependent on a previous instruction
									else rs[1].Vj = rf[inst_buffer[i].rs1]; //value in register at address rs1
								}
								rs[1].Dest = ROB[j].num;
								rs[1].A = inst_buffer[i].rs2_imm;
								rs[1].pc = inst_buffer[i].pc;
							}

							else if (inst_buffer[i].op == "sw") // check rs2, rs1 one more time
								if (rs[2].Busy == false) {
									rs[2].CC = 2;
									rs[2].Busy = true;
									rs[2].Op = "sw";
									for (int k = 0; k < 6; k++) {
										if (ROB[k].Dest == inst_buffer[i].rd) {
											rs[2].Qk = ROB[k].num;
											rs[2].Vk = -1;
											break;
										}// if dependent on a previous instruction
										else rs[2].Vk = rf[inst_buffer[i].rd];
									}
									for (int k = 0; k < 6; k++) {
										if (ROB[k].Dest == inst_buffer[i].rs1) {
											rs[2].Qj = ROB[k].num;
											rs[2].Vj = -1;
											break;
										}// if dependent on  a previous instruction
										else rs[2].Vj = rf[inst_buffer[i].rs1];
									}
									rs[2].A = inst_buffer[i].rs2_imm;
									rs[2].Dest = ROB[j].num;
									rs[2].pc = inst_buffer[i].pc;
								}
								else if (rs[3].Busy == false) {
									if (rs[3].Busy == false) {
										rs[3].CC = 2;
										rs[3].Busy = true;
										rs[3].Op = "sw";
										for (int k = 0; k < 6; k++) {
											if (ROB[k].Dest == inst_buffer[i].rsd) {
												rs[3].Qk = ROB[k].num;
												rs[3].Vk = -1;
												break;
											}// if dependent on previous instruction
											else rs[3].Vk = rf[inst_buffer[i].rsd];
										}
										for (int k = 0; k < 6; k++) {
											if (ROB[k].Dest == inst_buffer[i].rs1) {
												rs[3].Qj = ROB[k].num;
												rs[3].Vj = -1;
												break;
											}// if dependent on previous instruction
											else rs[3].Vj = rf[inst_buffer[i].rs1];
										}
										rs[3].A = inst_buffer[i].rs2_imm;
										rs[3].Dest = ROB[j].num;
										rs[3].pc = inst_buffer[i].pc;
									}

								}

								else if (inst_buffer[i].op == "jmp")
									if (rs[4].Busy == false) {
										rs[4].CC = 1;
										rs[4].Busy = true;
										rs[4].Op = "jmp";
										rs[4].A = inst_buffer[i].rs2_imm;
										rs[4].pc = inst_buffer[i].pc;
									}
									else if (rs[5].Busy == false) {
										rs[5].CC = 1;
										rs[5].Busy = true;
										rs[5].Op = "jmp";
										rs[5].A = inst_buffer[i].rs2_imm;
										rs[5].pc = inst_buffer[i].pc;
									}
									else if (rs[6].Busy == false) {
										rs[6].CC = 1;
										rs[6].Busy = true;
										rs[6].Op = "jmp";
										rs[6].A = inst_buffer[i].rs2_imm;
										rs[6].pc = inst_buffer[i].pc;
									}
									else if (inst_buffer[i].op == "jalr")
										if (rs[4].Busy == false) {
											rs[4].CC = 1;
											rs[4].Busy = true;
											rs[4].Op = "jalr";
											for (int k = 0; k < 6; k++) {
												if (ROB[k].Dest == inst_buffer[i].rs1) {
													rs[4].Qj = ROB[k].num;
													rs[4].Vj = -1;
													break;
												}// if dependent on previous instruction
												else rs[4].Vj = rf[inst_buffer[i].rs1];
											}
											rs[4].Dest = ROB[j].num;
											rs[4].pc = inst_buffer[i].pc;
										}
										else if (rs[5].Busy == false) {
											rs[5].CC = 1;
											rs[5].Busy = true;
											rs[5].Op = "jalr";
											for (int k = 0; k < 6; k++) {
												if (ROB[k].Dest == inst_buffer[i].rs1) {
													rs[5].Qj = ROB[k].num;
													rs[5].Vj = -1;
													break;
												}// if dependent on previous instruction
												else rs[5].Vj = rf[inst_buffer[i].rs1];
											}
											rs[5].Dest = ROB[j].num;
											rs[5].pc = inst_buffer[i].pc;
										}
										else if (rs[6].Busy == false) {
											rs[6].CC = 1;
											rs[6].Busy = true;
											rs[6].Op = "jalr";
											for (int k = 0; k < 6; k++) {
												if (ROB[k].Dest == inst_buffer[i].rs1) {
													rs[6].Qj = ROB[k].num;
													rs[6].Vj = -1;
													break;
												}// if dependent on previous instruction
												else rs[6].Vj = rf[inst_buffer[i].rs1];
											}
											rs[6].Dest = ROB[j].num;
											rs[6].pc = inst_buffer[i].pc;
										}
										else if (inst_buffer[i].op == "ret")
											if (rs[4].Busy == false) {
												rs[4].CC = 1;
												rs[4].Busy = true;
												rs[4].Op = "ret";
												rs[4].A = rf[inst_buffer[i].rs2_imm];
												rs[4].pc = inst_buffer[i].pc;
											}
											else if (rs[5].Busy == false) {
												rs[5].CC = 1;
												rs[5].Busy = true;
												rs[5].Op = "ret";
												rs[5].A = rf[inst_buffer[i].rs2_imm];
												rs[5].pc = inst_buffer[i].pc;
											}
											else if (rs[6].Busy == false) {
												rs[6].CC = 1;
												rs[6].Busy = true;
												rs[6].Op = "ret";
												rs[6].A = rf[inst_buffer[i].rs2_imm];
												rs[6].pc = inst_buffer[i].pc;
											}
											else if (inst_buffer[i].op == "beq")
												if (rs[7].Busy == false) {
													rs[7].CC = 1;
													rs[7].Busy = true;
													rs[7].Op = "beq";
													for (int k = 0; k < 6; k++) {
														if (ROB[k].Dest == inst_buffer[i].rd) {
															rs[7].Qk = ROB[k].num;
															rs[7].Vk = -1;
															break;
														}// if dependent on a previous instruction
														else rs[7].Vk = rf[inst_buffer[i].rd];
													}
													for (int k = 0; k < 6; k++) {
														if (ROB[k].Dest == inst_buffer[i].rs1) {
															rs[7].Qj = ROB[k].num;
															rs[7].Vj = -1;
															break;
														}// if dependent on  a previous instruction
														else rs[7].Vj = rf[inst_buffer[i].rs1];
													}
													rs[7].A = inst_buffer[i].rs2_imm;
													rs[7].Dest = ROB[j].num;
													rs[7].pc = inst_buffer[i].pc;
												}
												else if (rs[8].Busy == false) {
													rs[8].CC = 1;
													rs[8].Busy = true;
													rs[8].Op = "beq";
													for (int k = 0; k < 6; k++) {
														if (ROB[k].Dest == inst_buffer[i].rd) {
															rs[8].Qk = ROB[k].num;
															rs[8].Vk = -1;
															break;
														}// if dependent on a previous instruction
														else rs[8].Vk = rf[inst_buffer[i].rd];
													}
													for (int k = 0; k < 6; k++) {
														if (ROB[k].Dest == inst_buffer[i].rs1) {
															rs[8].Qj = ROB[k].num;
															rs[8].Vj = -1;
															break;
														}// if dependent on  a previous instruction
														else rs[8].Vj = rf[inst_buffer[i].rs1];
													}
													rs[8].A = inst_buffer[i].rs2_imm;
													rs[8].Dest = ROB[j].num;
													rs[8].pc = inst_buffer[i].pc;
												}
												else if (inst_buffer[i].op == "add" || inst_buffer[i].op == "sub")
													if (rs[9].Busy == false) {
														rs[9].CC = 2;
														rs[9].Busy = true;
														if (opcode == add_op)
															rs[9].Op = add_op;
														else if (opcode == sub_op)
															rs[9].Op = sub_op;
														for (int k = 0; k < 6; k++) {
															if (ROB[k].Dest == inst_buffer[i].rs1) {
																rs[9].Qj = ROB[k].num;
																rs[9].Vj = -1;
																break;
															}// if dependent on a previous instruction
															else rs[9].Vj = rf[inst_buffer[i].rs1];
														}
														for (int k = 0; k < 6; k++) {
															if (ROB[k].Dest == inst_buffer[i].rs2_imm) {
																rs[9].Qk = ROB[k].num;
																rs[9].Vk = -1;
																break;
															}// if dependent on  a previous instruction
															else rs[9].Vk = rf[inst_buffer[i].rs2_imm];
														}
														rs[9].Dest = ROB[j].num;
														rs[9].pc = inst_buffer[i].pc;

													}
													else if (rs[10].Busy == false) {
														rs[10].CC = 2;
														rs[10].Busy = true;
														if (opcode == add_op)
															rs[10].Op = add_op;
														else if (opcode == sub_op)
															rs[10].Op = sub_op;
														else rs[10].Op = addi_op;
														for (int k = 0; k < 6; k++) {
															if (ROB[k].Dest == inst_buffer[i].rs1) {
																rs[10].Qj = ROB[k].num;
																rs[10].Vj = -1;
																break;
															}// if dependent on a previous instruction
															else rs[10].Vj = rf[inst_buffer[i].rs1];
														}
														for (int k = 0; k < 6; k++) {
															if (ROB[k].Dest == inst_buffer[i].rs2_imm) {
																rs[10].Qk = ROB[k].num;
																rs[10].Vk = -1;
																break;
															}// if dependent on  a previous instruction
															else rs[10].Vk = rf[inst_buffer[i].rs2_imm];
														}
														rs[10].Dest = ROB[j].num;
														rs[10].pc = inst_buffer[i].pc;
													}
													else if (rs[11].Busy == false) {
														rs[11].CC = 2;
														rs[11].Busy = true;
														if (opcode == add_op)
															rs[11].Op = add_op;
														else if (opcode == sub_op)
															rs[11].Op = sub_op;
														else rs[11].Op = addi_op;
														for (int k = 0; k < 6; k++) {
															if (ROB[k].Dest == inst_buffer[i].rs1) {
																rs[11].Qj = ROB[k].num;
																rs[11].Vj = -1;
																break;
															}// if dependent on a previous instruction
															else rs[11].Vj = rf[inst_buffer[i].rs1];
														}
														for (int k = 0; k < 6; k++) {
															if (ROB[k].Dest == inst_buffer[i].rs2_imm) {
																rs[11].Qk = ROB[k].num;
																rs[11].Vk = -1;
																break;
															}// if dependent on  a previous instruction
															else rs[11].Vk = rf[inst_buffer[i].rs2_imm];
														}
														rs[11].Dest = ROB[j].num;
														rs[11].pc = inst_buffer[i].pc;
													}
													else if (inst_buffer[i].op == "addi")
														if (rs[9].Busy == false) {
															rs[9].CC = 2;
															rs[9].Busy = true;
															rs[9].Op = addi_op;
															for (int k = 0; k < 6; k++) {
																if (ROB[k].Dest == inst_buffer[i].rs1) {
																	rs[9].Qj = ROB[k].num;
																	rs[9].Vj = -1;
																	break;
																}// if dependent on a previous instruction
																else rs[9].Vj = rf[inst_buffer[i].rs1];
															}
															rs[9].Dest = ROB[j].num;
															rs[9].pc = inst_buffer[i].pc;
															rs[9].A = inst_buffer[i].rs2_imm;
														}
														else if (rs[10].Busy == false) {
															rs[10].CC = 2;
															rs[10].Busy = true;
															rs[10].Op = addi_op;
															for (int k = 0; k < 6; k++) {
																if (ROB[k].Dest == inst_buffer[i].rs1) {
																	rs[10].Qj = ROB[k].num;
																	rs[10].Vj = -1;
																	break;
																}// if dependent on a previous instruction
																else rs[10].Vj = rf[inst_buffer[i].rs1];
															}
															rs[10].Dest = ROB[j].num;
															rs[10].pc = inst_buffer[i].pc;
															rs[10].A = inst_buffer[i].rs2_imm;
														}
														else if (rs[11].Busy == false) {
															rs[11].CC = 2;
															rs[11].Busy = true;
															rs[11].Op = addi_op;
															for (int k = 0; k < 6; k++) {
																if (ROB[k].Dest == inst_buffer[i].rs1) {
																	rs[11].Qj = ROB[k].num;
																	rs[11].Vj = -1;
																	break;
																}// if dependent on a previous instruction
																else rs[11].Vj = rf[inst_buffer[i].rs1];
															}
															rs[11].Dest = ROB[j].num;
															rs[11].pc = inst_buffer[i].pc;
															rs[11].A = inst_buffer[i].rs2_imm;
														}
														else if (inst_buffer[i].op == "nand")
															if (rs[12].Busy == false) {
																rs[12].CC = 1;
																rs[12].Busy = true;
																rs[12].Op = nand_op;
																for (int k = 0; k < 6; k++) {
																	if (ROB[k].Dest == inst_buffer[i].rs1) {
																		rs[12].Qj = ROB[k].num;
																		rs[12].Vj = -1;
																		break;
																	}// if dependent on a previous instruction
																	else rs[12].Vj = rf[inst_buffer[i].rs1];
																}
																for (int k = 0; k < 6; k++) {
																	if (ROB[k].Dest == inst_buffer[i].rs2_imm) {
																		rs[12].Qk = ROB[k].num;
																		rs[12].Vk = -1;
																		break;
																	}// if dependent on  a previous instruction
																	else rs[12].Vk = rf[inst_buffer[i].rs2_imm];
																}
																rs[12].Dest = ROB[j].num;
																rs[12].pc = inst_buffer[i].pc;
															}
															else if (inst_buffer[i].op == "mult")
																if (rs[13].Busy == false) {
																	rs[13].CC = 8;
																	rs[13].Busy = true;
																	rs[13].Op = mult_op;
																	for (int k = 0; k < 6; k++) {
																		if (ROB[k].Dest == inst_buffer[i].rs1) {
																			rs[13].Qj = ROB[k].num;
																			rs[13].Vj = -1;
																			break;
																		}// if dependent on a previous instruction
																		else rs[13].Vj = rf[inst_buffer[i].rs1];
																	}
																	for (int k = 0; k < 6; k++) {
																		if (ROB[k].Dest == inst_buffer[i].rs2_imm) {
																			rs[13].Qk = ROB[k].num;
																			rs[13].Vk = -1;
																			break;
																		}// if dependent on  a previous instruction
																		else rs[13].Vk = rf[inst_buffer[i].rs2_imm];
																	}
																	rs[13].Dest = ROB[j].num;
																	rs[13].pc = inst_buffer[i].pc;
																}
																else if (rs[14].Busy == false) {
																	rs[14].CC = 8;
																	rs[14].Busy = true;
																	rs[14].Op = nand_op;
																	for (int k = 0; k < 6; k++) {
																		if (ROB[k].Dest == inst_buffer[i].rs1) {
																			rs[14].Qj = ROB[k].num;
																			rs[14].Vj = -1;
																			break;
																		}// if dependent on a previous instruction
																		else rs[14].Vj = rf[inst_buffer[i].rs1];
																	}
																	for (int k = 0; k < 6; k++) {
																		if (ROB[k].Dest == inst_buffer[i].rs2_imm) {
																			rs[14].Qk = ROB[k].num;
																			rs[14].Vk = -1;
																			break;
																		}// if dependent on  a previous instruction
																		else rs[14].Vk = rf[inst_buffer[i].rs2_imm];
																	}
																	rs[14].Dest = ROB[j].num;
																	rs[14].pc = inst_buffer[i].pc;
																}

					}

					//move inst_buffer head and clear current inst buffer
					inst_buffer[i].head = false;
					inst_buffer[i].op = "";
					inst_buffer[i].rd = "";
					inst_buffer[i].rs1 = "";
					inst_buffer[i].rs2_imm = "";
					// tail?    
					inst_buffer[(i + 1) % 4].head = true;
					break;

				}


			}
			//move ROB tail and add data 
			ROB[j].empty = false;
			ROB[j].tail = false;
			ROB[(j + 1) % 6] = true;
			ROB[j].Dest = inst_buffer[inst_buffer_index].rd;
			switch (opcode) {
			case lw_op:
				ROB[j].Type = lw_op;
			case sw_op:
				ROB[j].Type = sw_op;
			case jmp_op:
				ROB[j].Type = jmp_op;
			case jalr_op:
				ROB[j].Type = jalr_op;
			case ret_op:
				ROB[j].Type = ret_op;
			case beq_op:
				ROB[j].Type = beq_op;
			case add_op:
				ROB[j].Type = add_op;
			case sub_op:
				ROB[j].Type = sub_op;
			case addi_op:
				ROB[j].Type = addi_op;
			case nand_op:
				ROB[j].Type = nand_op;

			case mult_op:
				ROB[j].Type = mult_op;

			}
			//update rf ROB
			rf[inst_buffer[inst_buffer_index].rd].ROB_num = ROB[j].num;
			break;

		}

	}

}
}

void execute(Reservation_Station RS[])
{
	for (int i = 0; i < 15; i++)
	{

		if (RS[i].Busy == true)
			if (RS[i].Qj == -1 && RS[i].Qk == -1) {
				if (RS[i].station == "load1" || RS[i].station == "load2")
				{
					if (RS[i].CC == 2)
						RS[i].A = RS[i].Vj + RS[i].A;
					else if (RS[i].CC == 1) RS[i].result = mem[RS[i].A];
					RS[i].CC--;
				}
				if (RS[i].station == "store1" || RS[i].station == "store2") {
					if (RS[i].CC == 2)
						RS[i].A = RS[i].Vj + RS[i].A;

					RS[i].CC--;
				}
				if (RS[i].station == "jmp1" || RS[i].station == "jmp2" || RS[i].station == "jmp3") {
					///// ND 
					RS[i].CC--;
					if (RS[i].Op == jalr_op) { //
						RS[i].result = RS[i].pc + 1;
						address = RS[i].Vj;
					}
					else if (RS[i].Op = jmp_op) { //jump address 
						address = PC + 1 + imm;
					}
					else if (RS[i].Op = ret_op) { // return address saved in A parameter
						address = A;
					}

					RS[i].CC--;

				}
				if (RS[i].station == "beq1" || RS[i].station == "beq2") {
					if (RS[i].A < 0) branch_taken = true;
					else branch_taken = false;

					if (RS[i].Vj == RS[i].Vk) {
						act_branch_taken == true;
					}
					else act_branch_taken = false;
					/// (act_branch == branch_taked) 
					/// predicted ++
					/// else mispredicted++
					/// misprediction percentage = ( mispredicted / (predicted + mispredicted) ) *100

					if (branch_taken) {
						old_address = address;
						address = RS[i].A + RS[i].pc + 1;
					}

					RS[i].CC--;

				}
				if (RS[i].station == "add1" || RS[i].station == "add2" || RS[i].station == "add3") {
					if (RS[i].Op = add_op)
						RS[i].Dest = RS[i].Vj + RS[i].Vk;
					else if (RS[i].Op = sub_op)
						RS[i].Dest = RS[i].Vj - RS[i].Vk;
					else if (RS[i].Op = addi_op)
						RS[i].Dest = RS[i].Vj + RS[i].A;
					RS[i].CC--;
				}
				if (RS[i].station == "nand1") {
					RS[i].Dest = !(RS[i].Vj & RS[i].Vk);
					RS[i].CC--;
				}
				if (RS[i].station == "mul1" || RS[i].station == "mul2") {
					RS[i].Dest = RS[i].Vj * RS[i].Vk;
					RS[i].CC--;
				}
			}

	}
}


void writeback(vector<Reorder_Buffer>& ROB, Reservation_Station rs[]) {
	for (int i = 0; i < 15; i++) {
		if (rs[i].CC == 0 && rs[i].Busy == true) { //check if instruction finished execution
		  //should add 1 to clock value here?
			for (int k = 0; k < 6; k++) {
				if (ROB[k].Type == rs[i].Op && ROB[k].Dest == rs[i].Dest) { // find the inst in the ROB

					ROB[k].ready = true; //set ready and value
					ROB[k].Value = rs[i].result;
					break;

				}

				for (int j = 0; j < 15; j++) { //checking and updating for dependencies in reservation station
					if (rs[j].Qj == rs[i].Dest) {
						rs[j].Vj = ROB[k].Value;
						rs[j].Qj = -1;
					}
					if (rs[j].Qk == rs[i].Dest) {
						rs[j].Vk = ROB[k].Value;
						rs[j].Qk = -1;
					}

				}
			}
			//RE-INITIALIZE  current RS
			rs[i].Qj = -1;
			rs[i].Qk = -1;
			rs[i].Vj = -1;
			rs[i].Vk = -1;
			rs[i].Dest = -1;
			rs[i].A = -1;
			rs[i].Busy = false;
			rs[i].CC = 0;
			rs[i].result = -1;
			rs[i].Op = "";
			rs[i].pc = -1;


		}

	}

}



void commit(vector<Reorder_Buffer>& ROB, Registers regfile[]) {

	int curr;
	for (int i = 0; i < 6; i++) { //check all ROBs
		if (ROB[i].head == true && ROB[i].ready == true) {
			curr = ROB[i].num;
			if (ROB[i].Type != "beq") {
				beq_count++;
				if (branch_taken != act_branch_taken) {
					count_misprediction++;

					for (int k = 0; k < 5; k++) {
						ROB[(curr + k) % 6].ready = false;
						ROB[(curr + k) % 6].Dest = -1;
						ROB[(curr + k) % 6].Type = "";
						ROB[(curr + k) % 6].empty = true;
						ROB[(curr + k) % 6].Value = -1;
						ROB[(curr + k) % 6].head = false;
						ROB[(curr + k) % 6].tail = false;
					}
					for (int j = 0; j < 8; j++) {
						regfile[j].ROB_num = -1;
					}

					address = old_address;

				}

			}
			else if (ROB[i].Type == "sw") {
				mem[ROB[i].Value] = regfile[ROB[i].Dest]; //store instruction commit
				regfile[ROB[i].Dest].ROB_num = -1;
			}
			else {
				regfile[ROB[i].Dest].Val = ROB[i].Value; //rf[dest] = value  
				regfile[ROB[i].Dest].ROB_num = -1; //re-initilaize ROB_num

			}
			ROB[i].ready = false;
			ROB[i].head = false;
			ROB[i].Dest = -1;
			ROB[i].Type = "";
			ROB[i].Value = -1;
			ROB[(i + 1) % 6].head = true; // is this correct?
			instructions[address].is_committed = true; // when do we use this
			if (commit_count++ == 2) break;
		}
	}

}


int main() {

	float misprediction = 0;
	float IPC = 0;


	get_inst_data(); // function to retrieve instructions and data

	Reservation_Station rs[15];
	for (int i = 0; i < 15; i++) {
		rs[i].A = -1;
		rs[i].Busy = false;
		rs[i].CC = 0;
		rs[i].Op = "";
		if (i == 0)rs[i].station = "load1";
		else if (i == 1)rs[i].station = "load2";
		else if (i == 2)rs[i].station = "store1";
		else if (i == 3)rs[i].station = "store2";
		else if (i == 4)rs[i].station = "jmp1";
		else if (i == 5)rs[i].station = "jmp2";
		else if (i == 6)rs[i].station = "jmp3";
		else if (i == 7)rs[i].station = "beq1";
		else if (i == 8)rs[i].station = "beq2";
		else if (i == 9)rs[i].station = "add1";
		else if (i == 10)rs[i].station = "add2";
		else if (i == 11)rs[i].station = "add3";
		else if (i == 12)rs[i].station = "nand1";
		else if (i == 13)rs[i].station = "mul1";
		else if (i == 14)rs[i].station = "mul2";
		rs[i].Qj = -1;
		rs[i].Qk = -1;
		rs[i].Vj = -1;
		rs[i].Vk = -1;
		rs[i].Dest = -1;
		rs[i].result = -1;
		rs[i].pc = -1;
	}

	vector <Instruction_Buffer> Inst_buffer;
	// INITIALIZING INSTRUCTION BUFFER
	Inst_buffer.push_back(Instruction_Buffer{ true, true, 1, "", 0, 0, 0 });

	for (int i = 1; i < 4; i++) {
		Inst_buffer.push_back(Instruction_Buffer{ false, false, i + 1, "", 0, 0, 0 });
	}

	vector<Reorder_Buffer> ROB;
	// INITIALIZING ROB
	ROB.push_back(Reorder_Buffer{ true, true, 1, "", 0, 0, false , true });

	for (int i = 1; i < 5; i++) {
		ROB.push_back(Reorder_Buffer{ false, false, i + 1, "", 0, 0, false, true });
	}

	Registers rf[8] = { Registers{ 0, -1 } };

	do {
		commit(ROB, rf);
		writeback(ROB, rs);
		execute(rs);
		issue(inst_buffer, ROB, rs, rf);
		Fetch(Inst_buffer, ROB, rs);

		sys_clk++;
	} while (instructions.back().is_committed != true);
	misprediction = misprediction_count / beq_count * 100;
	IPC = instruction_count / sys_clk;

	cout << "clock cycles: " << sys_clk << endl;
	cout << "IPC: " << IPC << endl;
	cout << "misprediction percentage: " << misprediction << endl;



}
